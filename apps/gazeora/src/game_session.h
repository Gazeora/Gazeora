#pragma once

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "gazeora/host/stream/frame_stream.h"
#include "gazeora/host/launch/launcher.h"

namespace gazeora::app {

// One game started from the UI. Launching (which may wait on the hook or on a launcher
// handing off to the real game), watching the process and receiving captured frames all
// happen on a worker thread. Captured frames go to the shared stream server while someone
// is watching it.
class GameSession {
public:
    enum class State {
        Launching,
        WaitingForGame,  // started through Steam or a launcher; waiting for the game exe
        Running,
        Exited,
        Failed,
        Cancelled,
    };

    struct Stats {
        State state = State::Launching;
        std::wstring error;  // when Failed
        DWORD pid = 0;
        int64_t elapsedSeconds = 0;
        // Capture only:
        LONG hookState = 0;
        std::string hookStatus;
        double fps = 0.0;
        double handoffMs = 0.0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t api = 0;
        int64_t skippedFrames = 0;
        std::wstring streamError;  // why frames can't be streamed, if they can't
        bool stereoWanted = false;  // 3D is on (VrLayout::stereo)
        bool stereo = false;        // and the last frame streamed was in 3D
    };

    // `stream` must outlive the session; null disables streaming.
    GameSession(uint32_t gameId, const LaunchSpec& spec, bool capture, FrameStreamServer* stream);
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;
    // Stops watching; the game itself keeps running.
    ~GameSession();

    uint32_t GameId() const { return m_gameId; }
    bool Capture() const { return m_capture; }
    // The exe the session waits for while WaitingForGame.
    std::wstring GameProcessName() const { return m_spec.GameProcessName(); }
    bool ThroughSteam() const { return !m_spec.steamAppId.empty(); }
    Stats Snapshot() const;
    bool Finished() const;

    // Stops waiting for a game that hasn't started yet.
    void Cancel() { m_stop = true; }

private:
    void Run();
    // Watches `game` until it exits or the session stops. Returns true if it exited.
    bool Watch(LaunchedGame& game, CaptureClient* client);
    void RunCapture(CaptureClient& client, LaunchedGame& game);
    void SetState(State state, const std::wstring& error = {});
    int64_t ElapsedSeconds() const;

    const uint32_t m_gameId;
    const LaunchSpec m_spec;
    const bool m_capture;
    FrameStreamServer* const m_stream;
    LARGE_INTEGER m_start = {};
    LARGE_INTEGER m_end = {};
    LONGLONG m_qpcFreq = 1;

    std::atomic<bool> m_stop{false};
    mutable std::mutex m_mutex;  // guards m_stats, m_start/m_end
    Stats m_stats;
    std::thread m_thread;
};

}  // namespace gazeora::app
