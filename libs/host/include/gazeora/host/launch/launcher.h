#pragma once

#include <windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <utility>

#include "gazeora/host/capture/capture_client.h"

namespace gazeora {

struct LaunchSpec {
    std::wstring exePath;
    std::wstring arguments;   // command line after the exe, as typed
    std::wstring workingDir;  // empty = the exe's folder
    // Non-empty: start through Steam (steam://rungameid/<id>) instead of running the exe.
    std::wstring steamAppId;
    // The exe name of the real game process, when it differs from exePath's (e.g. a
    // launcher.exe that starts game.exe). Empty = exePath's file name.
    std::wstring processName;

    std::wstring GameProcessName() const;
};

// A game whose process exits this soon after starting is assumed to have handed off to a
// launcher (Steam, Ubisoft Connect, EA app, ...) that starts the game again.
constexpr DWORD kLauncherHandOffMs = 20000;

// How long to wait for the real game process after a hand-off. Launchers may show a login
// or update screen first.
constexpr DWORD kWaitForGameMs = 5 * 60 * 1000;

// A started game. Owns the process handle.
struct LaunchedGame {
    DWORD pid = 0;
    HANDLE process = nullptr;

    LaunchedGame() = default;
    LaunchedGame(const LaunchedGame&) = delete;
    LaunchedGame& operator=(const LaunchedGame&) = delete;
    LaunchedGame(LaunchedGame&& other) noexcept { *this = std::move(other); }
    LaunchedGame& operator=(LaunchedGame&& other) noexcept;
    ~LaunchedGame();

    bool Running() const { return process && WaitForSingleObject(process, 0) == WAIT_TIMEOUT; }
};

// Tells a game that handed off to its launcher from one that was played and closed. A
// launcher stub (Steam's restart, Ubisoft Connect, EA app) exits within kLauncherHandOffMs
// without ever showing a window; a game shows one, however soon it's closed. Watches the
// process on a background thread from construction until it exits, shows a window, or
// kLauncherHandOffMs passes.
class HandOffDetector {
public:
    // `process` is not owned and must stay open while the detector lives.
    HandOffDetector(DWORD pid, HANDLE process);
    HandOffDetector(const HandOffDetector&) = delete;
    HandOffDetector& operator=(const HandOffDetector&) = delete;
    ~HandOffDetector();

    bool ShowedWindow() const { return m_showedWindow; }
    // Call once the process has exited: true if it left early without showing a window, so
    // the real game should start again from its launcher.
    bool HandedOff();

private:
    void Run();

    const DWORD m_pid;
    const HANDLE m_process;
    const ULONGLONG m_startedAt;
    std::atomic<bool> m_showedWindow{false};
    std::atomic<bool> m_stop{false};
    std::thread m_thread;
};

// The Steam app id of the game whose exe this is (from steamapps/appmanifest_<id>.acf),
// or empty if the exe isn't in a Steam library.
std::wstring FindSteamAppId(const std::wstring& exePath);

// Asks Steam to start the game. There is no process to track until it appears.
bool LaunchThroughSteam(const std::wstring& appId, std::wstring& error);

// Waits for a process named `exeName` (other than `ignorePid`) to start, then until it is
// safe to inject: it has finished starting up (shows a window) and loaded a graphics
// runtime, or `readyTimeoutMs` passed. That is before it creates its swap chain in most
// games. A process that exits meanwhile is taken as another launcher stage and skipped.
// Returns false on timeout or when `cancelled` returns true.
bool WaitForGameProcess(const std::wstring& exeName, DWORD ignorePid, DWORD timeoutMs,
                        DWORD readyTimeoutMs, const std::function<bool()>& cancelled,
                        LaunchedGame& game, std::wstring& error);

// Injects the hook into an already running game, creating the shared state on `client`.
bool AttachHook(DWORD pid, const std::wstring& dllPath, CaptureClient& client,
                std::wstring& error);

// Starts the game without injecting anything.
bool LaunchGame(const LaunchSpec& spec, LaunchedGame& game, std::wstring& error);

// Starts the game suspended, creates the shared state on `client`, injects the hook
// before any game code runs and resumes the game once the hook has set itself up.
bool LaunchGameWithHook(const LaunchSpec& spec, const std::wstring& dllPath,
                        CaptureClient& client, LaunchedGame& game, std::wstring& error);

}  // namespace gazeora
