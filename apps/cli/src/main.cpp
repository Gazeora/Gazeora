// GazeoraCLI: command-line front end. Manages the same game library as Gazeora.exe,
// launches games, and injects the capture hook into a game to receive its frames.
//
//   GazeoraCLI list
//   GazeoraCLI add <game.exe | shortcut.lnk> [--name N] [--args A] [--workdir D] [--capture]
//   GazeoraCLI remove <id | name>
//   GazeoraCLI launch <id | name> [--capture]
//   GazeoraCLI attach <pid | process.exe>
//
// Keys while capturing:  S = save current frame as BMP,  Q = quit.

#include <windows.h>
#include <conio.h>
#include <objbase.h>

#include <cstdio>
#include <cwctype>
#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/capture/capture_client.h"
#include "gazeora/host/capture/frame_dump.h"
#include "gazeora/host/launch/injector.h"
#include "gazeora/host/launch/launcher.h"
#include "gazeora/library/game_library.h"

namespace {

volatile bool g_quit = false;

BOOL WINAPI OnConsoleCtrl(DWORD) {
    g_quit = true;
    return TRUE;
}

void PrintUsage(const wchar_t* exe) {
    fwprintf(stderr,
             L"usage:\n"
             L"  %ls list\n"
             L"  %ls add <game.exe | shortcut.lnk> [--name N] [--args A] [--workdir D] "
             L"[--capture]\n"
             L"  %ls remove <id | name>\n"
             L"  %ls launch <id | name> [--capture]\n"
             L"  %ls attach <pid | process.exe>\n",
             exe, exe, exe, exe, exe);
}

bool IsNumber(const std::wstring& text) {
    bool numeric = !text.empty();
    for (wchar_t c : text) {
        numeric = numeric && iswdigit(c);
    }
    return numeric;
}

DWORD ResolveTarget(const std::wstring& arg) {
    return IsNumber(arg) ? wcstoul(arg.c_str(), nullptr, 10) : gazeora::FindProcessByName(arg);
}

std::wstring FullPath(const std::wstring& path) {
    wchar_t full[MAX_PATH * 2];
    DWORD length = GetFullPathNameW(path.c_str(), ARRAYSIZE(full), full, nullptr);
    return length > 0 && length < ARRAYSIZE(full) ? std::wstring(full) : path;
}

bool EndsWithI(const std::wstring& text, const wchar_t* suffix) {
    size_t n = wcslen(suffix);
    return text.size() >= n && _wcsicmp(text.c_str() + text.size() - n, suffix) == 0;
}

const char* HookStateName(LONG state) {
    switch (state) {
        case gazeora::kHookNotLoaded:
            return "not loaded";
        case gazeora::kHookInitializing:
            return "initializing";
        case gazeora::kHookInstalled:
            return "hooked, waiting for Present";
        case gazeora::kHookCapturing:
            return "capturing";
        case gazeora::kHookError:
            return "error";
        default:
            return "unknown";
    }
}

std::wstring FormatPlaytime(int64_t seconds) {
    if (seconds <= 0) {
        return L"-";
    } else if (seconds < 60) {
        return L"< 1m";
    }
    wchar_t text[32];
    swprintf(text, 32, L"%lldh %02lldm", seconds / 3600, (seconds / 60) % 60);
    return text;
}

// Prints capture stats until Q / Ctrl+C, or until `game` (if given) exits.
int RunCaptureLoop(gazeora::CaptureClient& client, HANDLE game) {
    client.SetCaptureEnabled(true);
    SetConsoleCtrlHandler(OnConsoleCtrl, TRUE);
    printf("Press S to save a frame, Q to quit.\n");

    LARGE_INTEGER freq, statsStart, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&statsStart);
    int frames = 0;
    double handoffSum = 0.0;
    LONG lastState = -100;
    std::string lastStatus;
    std::wstring error;

    while (!g_quit) {
        if (client.WaitForFrame(100) == gazeora::CaptureClient::WaitResult::Frame) {
            ++frames;
            handoffSum += client.LastHandoffMs();
        }
        if (game && WaitForSingleObject(game, 0) == WAIT_OBJECT_0) {
            printf("Game exited.\n");
            break;
        }

        const gazeora::SharedHeader& shared = client.Shared();
        std::string status(shared.status, strnlen(shared.status, sizeof(shared.status)));
        if (shared.hookState != lastState || status != lastStatus) {
            lastState = shared.hookState;
            lastStatus = status;
            printf("[hook] %s%s%s\n", HookStateName(lastState), status.empty() ? "" : ": ",
                   status.c_str());
        }

        QueryPerformanceCounter(&now);
        double elapsed = double(now.QuadPart - statsStart.QuadPart) / double(freq.QuadPart);
        if (elapsed >= 1.0) {
            if (frames > 0) {
                printf("%5.1f fps  %ux%u %s  handoff %.2f ms  skipped %lld\n", frames / elapsed,
                       shared.width, shared.height, gazeora::GraphicsApiName(shared.api),
                       handoffSum / frames, shared.skippedFrames);
            }
            frames = 0;
            handoffSum = 0.0;
            statsStart = now;
        }

        while (_kbhit()) {
            int key = towupper(_getwch());
            if (key == L'Q') {
                g_quit = true;
            } else if (key == L'S') {
                std::wstring path = L"frame_" + std::to_wstring(shared.frameIndex) + L".bmp";
                if (gazeora::SaveTextureAsBmp(client.Device(), client.Frame(), path, error)) {
                    wprintf(L"Saved %ls\n", path.c_str());
                } else {
                    fwprintf(stderr, L"Save failed: %ls\n", error.c_str());
                }
            }
        }
    }

    // The hook stays loaded (it can't be safely unloaded); it just stops copying.
    client.SetCaptureEnabled(false);
    printf("Capture stopped.\n");
    return 0;
}

int CmdAttach(const std::wstring& target) {
    DWORD pid = ResolveTarget(target);
    if (pid == 0) {
        fwprintf(stderr, L"process '%ls' not found\n", target.c_str());
        return 1;
    }

    std::wstring dllPath = gazeora::HookDllPathNextToExe();
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr, L"hook DLL not found at %ls\n", dllPath.c_str());
        return 1;
    }

    gazeora::CaptureClient client;
    std::wstring error;
    if (!client.Create(pid, error)) {
        fwprintf(stderr, L"%ls\n", error.c_str());
        return 1;
    }

    if (client.HookAlreadyLoaded()) {
        printf("Hook already loaded in PID %lu, reattaching\n", pid);
    } else {
        printf("Injecting into PID %lu...\n", pid);
        if (!gazeora::InjectDll(pid, dllPath, error)) {
            fwprintf(stderr, L"injection failed: %ls\n", error.c_str());
            return 1;
        }
    }
    return RunCaptureLoop(client, nullptr);
}

bool LoadLibraryOrReport(gazeora::GameLibrary& library) {
    std::wstring error;
    if (!library.Load(error)) {
        fwprintf(stderr, L"%ls\n", error.c_str());
        return false;
    }
    return true;
}

bool SaveLibraryOrReport(gazeora::GameLibrary& library) {
    std::wstring error;
    if (!library.Save(error)) {
        fwprintf(stderr, L"%ls\n", error.c_str());
        return false;
    }
    return true;
}

int CmdList() {
    gazeora::GameLibrary library;
    if (!LoadLibraryOrReport(library)) {
        return 1;
    }
    if (library.Games().empty()) {
        wprintf(L"No games yet. Add one with: GazeoraCLI add <game.exe>\n");
        return 0;
    }
    wprintf(L"%4ls  %-32ls  %-9ls  %-8ls  %-14ls  %ls\n", L"ID", L"NAME", L"PLAYTIME",
            L"CAPTURE", L"LAUNCH", L"PATH");
    for (const gazeora::Game& game : library.Games()) {
        gazeora::LaunchSpec spec = gazeora::ToLaunchSpec(game);
        std::wstring via = spec.steamAppId.empty() ? L"exe" : L"steam " + spec.steamAppId;
        wprintf(L"%4u  %-32ls  %-9ls  %-8ls  %-14ls  %ls\n", game.id, game.name.c_str(),
                FormatPlaytime(game.playtimeSeconds).c_str(), game.captureOnLaunch ? L"on" : L"off",
                via.c_str(), game.exePath.c_str());
    }
    return 0;
}

int CmdAdd(const std::vector<std::wstring>& args) {
    if (args.empty()) {
        fwprintf(stderr, L"add: missing path to the game's .exe or .lnk\n");
        return 1;
    }
    gazeora::Game game;
    std::wstring path = FullPath(args[0]);
    for (size_t i = 1; i < args.size(); ++i) {
        bool hasValue = i + 1 < args.size();
        if (args[i] == L"--name" && hasValue) {
            game.name = args[++i];
        } else if (args[i] == L"--args" && hasValue) {
            game.arguments = args[++i];
        } else if (args[i] == L"--workdir" && hasValue) {
            game.workingDir = FullPath(args[++i]);
        } else if (args[i] == L"--capture") {
            game.captureOnLaunch = true;
        } else {
            fwprintf(stderr, L"add: unknown option '%ls'\n", args[i].c_str());
            return 1;
        }
    }

    if (EndsWithI(path, L".lnk")) {
        gazeora::Game resolved = game;
        if (!gazeora::ResolveShortcut(path, resolved)) {
            fwprintf(stderr, L"add: '%ls' does not point at a program\n", path.c_str());
            return 1;
        }
        // Options given on the command line win over the shortcut's.
        if (!game.arguments.empty()) resolved.arguments = game.arguments;
        if (!game.workingDir.empty()) resolved.workingDir = game.workingDir;
        game = resolved;
    } else {
        game.exePath = path;
    }
    if (GetFileAttributesW(game.exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr, L"add: '%ls' not found\n", game.exePath.c_str());
        return 1;
    }

    gazeora::GameLibrary library;
    if (!LoadLibraryOrReport(library)) {
        return 1;
    }
    uint32_t id = library.Add(game);
    if (!SaveLibraryOrReport(library)) {
        return 1;
    }
    wprintf(L"Added [%u] %ls\n", id, library.Find(id)->name.c_str());
    return 0;
}

int CmdRemove(const std::wstring& key) {
    gazeora::GameLibrary library;
    if (!LoadLibraryOrReport(library)) {
        return 1;
    }
    gazeora::Game* game = library.FindByIdOrName(key);
    if (!game) {
        fwprintf(stderr, L"no single game matches '%ls' (see GazeoraCLI list)\n", key.c_str());
        return 1;
    }
    std::wstring name = game->name;
    library.Remove(game->id);
    if (!SaveLibraryOrReport(library)) {
        return 1;
    }
    wprintf(L"Removed %ls\n", name.c_str());
    return 0;
}

int CmdLaunch(const std::wstring& key, bool forceCapture) {
    gazeora::GameLibrary library;
    if (!LoadLibraryOrReport(library)) {
        return 1;
    }
    gazeora::Game* game = library.FindByIdOrName(key);
    if (!game) {
        fwprintf(stderr, L"no single game matches '%ls' (see GazeoraCLI list)\n", key.c_str());
        return 1;
    }
    gazeora::LaunchSpec spec = gazeora::ToLaunchSpec(*game);
    bool capture = forceCapture || game->captureOnLaunch;
    uint32_t id = game->id;
    std::wstring name = game->name;
    bool throughSteam = !spec.steamAppId.empty();

    game->lastPlayed = gazeora::UnixNow();
    SaveLibraryOrReport(library);

    std::wstring error;
    gazeora::LaunchedGame launched;
    if (!capture) {
        // Nothing to watch: launchers that restart the game carry on by themselves.
        bool ok = throughSteam ? gazeora::LaunchThroughSteam(spec.steamAppId, error)
                               : gazeora::LaunchGame(spec, launched, error);
        if (!ok) {
            fwprintf(stderr, L"launch failed: %ls\n", error.c_str());
            return 1;
        }
        if (throughSteam) {
            wprintf(L"Started %ls through Steam (app %ls)\n", name.c_str(),
                    spec.steamAppId.c_str());
        } else {
            wprintf(L"Launched %ls (PID %lu)\n", name.c_str(), launched.pid);
        }
        return 0;
    }

    SetConsoleCtrlHandler(OnConsoleCtrl, TRUE);
    std::wstring dllPath = gazeora::HookDllPathNextToExe();
    auto client = std::make_unique<gazeora::CaptureClient>();
    bool waitForGame = false;  // no process yet: find the game by name, then attach
    DWORD ignorePid = 0;
    if (throughSteam) {
        if (!gazeora::LaunchThroughSteam(spec.steamAppId, error)) {
            fwprintf(stderr, L"launch failed: %ls\n", error.c_str());
            return 1;
        }
        wprintf(L"Started %ls through Steam (app %ls)\n", name.c_str(), spec.steamAppId.c_str());
        waitForGame = true;
    } else {
        if (!gazeora::LaunchGameWithHook(spec, dllPath, *client, launched, error)) {
            fwprintf(stderr, L"launch failed: %ls\n", error.c_str());
            return 1;
        }
        wprintf(L"Launched %ls (PID %lu) with capture\n", name.c_str(), launched.pid);
    }

    LARGE_INTEGER start, end, freq;
    QueryPerformanceFrequency(&freq);
    int result = 0;
    for (int handOffs = 0; handOffs <= 4 && !g_quit; ++handOffs) {
        if (waitForGame) {
            wprintf(L"Waiting for %ls to start (Ctrl+C to stop)...\n",
                    spec.GameProcessName().c_str());
            if (!gazeora::WaitForGameProcess(spec.GameProcessName(), ignorePid,
                                             gazeora::kWaitForGameMs, 30000,
                                             [] { return bool(g_quit); }, launched, error)) {
                fwprintf(stderr, L"the game didn't start: %ls\n", error.c_str());
                return 1;
            }
            client = std::make_unique<gazeora::CaptureClient>();
            if (!gazeora::AttachHook(launched.pid, dllPath, *client, error)) {
                fwprintf(stderr, L"attaching capture failed: %ls\n", error.c_str());
                return 1;
            }
            wprintf(L"Attached to %ls (PID %lu)\n", spec.GameProcessName().c_str(),
                    launched.pid);
        }

        QueryPerformanceCounter(&start);
        result = RunCaptureLoop(*client, launched.process);
        QueryPerformanceCounter(&end);
        LONGLONG ms = (end.QuadPart - start.QuadPart) * 1000 / freq.QuadPart;
        if (g_quit || launched.Running() || ms > LONGLONG(gazeora::kLauncherHandOffMs)) {
            break;
        }
        // Gone right after starting: it most likely restarted through its launcher.
        printf("The game restarted through its launcher; following it.\n");
        ignorePid = launched.pid;
        waitForGame = true;
    }

    // We watched the whole session, so we know how long it was played.
    if (!launched.Running() && LoadLibraryOrReport(library)) {
        if (gazeora::Game* played = library.Find(id)) {
            played->playtimeSeconds += (end.QuadPart - start.QuadPart) / freq.QuadPart;
            SaveLibraryOrReport(library);
        }
    }
    return result;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);  // for .lnk shortcuts
    // Status lines should show up as they happen, also when piped or redirected.
    setvbuf(stdout, nullptr, _IONBF, 0);

    std::wstring command = argv[1];
    std::vector<std::wstring> args(argv + 2, argv + argc);
    if (command == L"list") {
        return CmdList();
    } else if (command == L"add") {
        return CmdAdd(args);
    } else if (command == L"remove" && args.size() == 1) {
        return CmdRemove(args[0]);
    } else if (command == L"launch" && !args.empty()) {
        bool capture = args.size() > 1 && args[1] == L"--capture";
        return CmdLaunch(args[0], capture);
    } else if (command == L"attach" && args.size() == 1) {
        return CmdAttach(args[0]);
    } else if (command == L"help" || command == L"--help" || command == L"-h") {
        PrintUsage(argv[0]);
        return 0;
    } else if (args.empty() && command[0] != L'-') {
        // The original interface: GazeoraCLI <pid | process.exe>.
        return CmdAttach(command);
    }
    PrintUsage(argv[0]);
    return 1;
}
