#include "game_session.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "gazeora/host/capture/capture_client.h"
#include "gazeora/host/capture/frame_dump.h"  // TextureReader
#include "gazeora/host/launch/injector.h"
#include "gazeora/library/game_library.h"

namespace gazeora::app {

namespace {

// Launcher hand-offs to follow before giving up (stub -> launcher -> game is two).
constexpr int kMaxHandOffs = 4;

// After the game process appears, how long to wait for it to finish starting up (window
// and graphics runtime) before injecting anyway.
constexpr DWORD kWaitForReadyMs = 30000;

// Frames handed to the stream server per second, at most; more is a lot for Wi-Fi.
constexpr int kStreamMaxFps = 30;

}  // namespace

GameSession::GameSession(uint32_t gameId, const LaunchSpec& spec, bool capture,
                         FrameStreamServer* stream)
    : m_gameId(gameId), m_spec(spec), m_capture(capture), m_stream(stream) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_qpcFreq = freq.QuadPart;
    QueryPerformanceCounter(&m_start);
    m_thread = std::thread([this] { Run(); });
}

GameSession::~GameSession() {
    m_stop = true;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

GameSession::Stats GameSession::Snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    Stats stats = m_stats;
    stats.elapsedSeconds = stats.state == State::Running || stats.state == State::Exited
                               ? ElapsedSeconds()
                               : 0;
    return stats;
}

bool GameSession::Finished() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats.state == State::Exited || m_stats.state == State::Failed ||
           m_stats.state == State::Cancelled;
}

int64_t GameSession::ElapsedSeconds() const {
    LARGE_INTEGER now = m_end;
    if (now.QuadPart == 0) {
        QueryPerformanceCounter(&now);
    }
    return (now.QuadPart - m_start.QuadPart) / m_qpcFreq;
}

void GameSession::SetState(State state, const std::wstring& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.state = state;
    m_stats.error = error;
    if (state == State::Exited) {
        QueryPerformanceCounter(&m_end);
    }
}

void GameSession::Run() {
    std::wstring error;
    std::wstring dllPath = HookDllPathNextToExe();
    LaunchedGame game;
    auto client = std::make_unique<CaptureClient>();
    bool captureReady = m_capture;
    bool waitForGame = false;  // no process to watch yet: find it by name
    DWORD ignorePid = 0;

    if (ThroughSteam()) {
        if (!LaunchThroughSteam(m_spec.steamAppId, error)) {
            SetState(State::Failed, error);
            return;
        }
        waitForGame = true;
    } else if (!(m_capture ? LaunchGameWithHook(m_spec, dllPath, *client, game, error)
                           : LaunchGame(m_spec, game, error))) {
        SetState(State::Failed, error);
        return;
    }

    for (int handOffs = 0; handOffs <= kMaxHandOffs; ++handOffs) {
        if (waitForGame) {
            SetState(State::WaitingForGame);
            if (!WaitForGameProcess(m_spec.GameProcessName(), ignorePid, kWaitForGameMs,
                                    kWaitForReadyMs, [this] { return m_stop.load(); }, game,
                                    error)) {
                SetState(m_stop ? State::Cancelled : State::Failed,
                         L"the game didn't start: " + error);
                return;
            }
            if (m_capture) {
                // The hook goes into the running game rather than at process creation.
                client = std::make_unique<CaptureClient>();
                captureReady = AttachHook(game.pid, dllPath, *client, error);
                if (!captureReady) {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_stats.hookState = kHookError;
                    m_stats.hookStatus = "capture unavailable: " + WideToUtf8(error);
                }
            }
        }

        HandOffDetector handOff(game.pid, game.process);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            QueryPerformanceCounter(&m_start);  // playtime counts from the real game starting
            m_stats.state = State::Running;
            m_stats.pid = game.pid;
        }
        if (!Watch(game, captureReady ? client.get() : nullptr)) {
            return;  // the session was stopped; the game keeps running
        }
        if (!handOff.HandedOff()) {
            break;  // played and closed, however briefly: it showed a window
        }
        // Gone right after starting, without a window: most likely it restarted itself
        // through its launcher (Steam, Ubisoft Connect, EA app...). Follow it to the real
        // game process.
        ignorePid = game.pid;
        waitForGame = true;
    }
    SetState(State::Exited);
}

bool GameSession::Watch(LaunchedGame& game, CaptureClient* client) {
    if (client) {
        RunCapture(*client, game);
    } else {
        while (!m_stop && WaitForSingleObject(game.process, 250) == WAIT_TIMEOUT) {
        }
    }
    return !game.Running();
}

void GameSession::RunCapture(CaptureClient& client, LaunchedGame& game) {
    client.SetCaptureEnabled(true);
    LARGE_INTEGER statsStart, now;
    QueryPerformanceCounter(&statsStart);
    int frames = 0;
    double handoffSum = 0.0;
    TextureReader reader;
    reader.SetMaxSize(FrameStreamServer::kMaxWidth, FrameStreamServer::kMaxHeight);
    std::vector<uint8_t> pixels;
    LONGLONG nextStream = 0;     // QPC time the next streamed frame is due
    LONGLONG lastGameFrame = 0;  // QPC time of the game's latest frame
    const LONGLONG interval = m_qpcFreq / kStreamMaxFps;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.fps = 0.0;
        m_stats.handoffMs = 0.0;
    }

    while (!m_stop && game.Running()) {
        // Wait for the game's next frame, but no later than the end of the current slot, when
        // the last frame may have to go out again.
        QueryPerformanceCounter(&now);
        DWORD waitMs = 100;
        if (nextStream && lastGameFrame) {
            LONGLONG untilOverdue = nextStream + interval / 4 - now.QuadPart;
            waitMs = DWORD(std::clamp<LONGLONG>(untilOverdue * 1000 / m_qpcFreq + 1, 1, 100));
        }
        bool gotFrame = client.WaitForFrame(waitMs) == CaptureClient::WaitResult::Frame;
        if (gotFrame) {
            ++frames;
            handoffSum += client.LastHandoffMs();
        }
        const SharedHeader& shared = client.Shared();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.hookState = shared.hookState;
            m_stats.hookStatus.assign(shared.status, strnlen(shared.status, sizeof(shared.status)));
            m_stats.width = shared.width;
            m_stats.height = shared.height;
            m_stats.api = shared.api;
            m_stats.skippedFrames = shared.skippedFrames;
        }

        QueryPerformanceCounter(&now);
        if (gotFrame) {
            lastGameFrame = now.QuadPart;
        }
        // Frames go out on a fixed schedule, taking the first game frame within a quarter
        // interval of each slot. "An interval since the last one" would skip a 60 fps
        // game's frames that land a hair early and drop the stream to 20 fps.
        // A game below 30 fps, or one that stalls for a moment, leaves slots empty; the last
        // frame goes out again to fill them, as players such as VLC drop frames as late when a
        // stream runs slow. After a second without game frames the server shows the logo.
        bool due = now.QuadPart >= nextStream - interval / 4;
        bool repeat = !gotFrame && lastGameFrame && now.QuadPart - lastGameFrame < m_qpcFreq &&
                      now.QuadPart >= nextStream + interval / 4;
        if (((gotFrame && due) || repeat) && m_stream && m_stream->WantsFrames()) {
            // After a pause the schedule restarts from now instead of catching up.
            nextStream = std::max(nextStream, now.QuadPart - interval / 4) + interval;
            UINT width, height;
            std::wstring error;
            reader.SetVrLayout(m_stream->GetVrLayout());  // may change while playing
            if (reader.Read(client.Device(), client.Frame(), width, height, pixels, error)) {
                m_stream->PushFrame(width, height, pixels);
            }
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.streamError = error;
        }

        double elapsed = double(now.QuadPart - statsStart.QuadPart) / double(m_qpcFreq);
        if (elapsed >= 1.0) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.fps = frames / elapsed;
            m_stats.handoffMs = frames > 0 ? handoffSum / frames : 0.0;
            frames = 0;
            handoffSum = 0.0;
            statsStart = now;
        }
    }
    // The hook stays loaded (it can't be safely unloaded); it just stops copying.
    client.SetCaptureEnabled(false);
}

}  // namespace gazeora::app
