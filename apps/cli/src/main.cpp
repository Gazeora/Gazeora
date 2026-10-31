// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// GazeoraCLI: command-line front end. Manages the same game library as Gazeora.exe,
// launches games, and injects the capture hook into a game to receive its frames.
//
//   GazeoraCLI list
//   GazeoraCLI add <game.exe | shortcut.lnk> [--name N] [--args A] [--workdir D] [--capture]
//   GazeoraCLI remove <id | name>
//   GazeoraCLI launch <id | name> [--capture]
//   GazeoraCLI attach <pid | process.exe> [--depth] [--save-after SECONDS]
//   GazeoraCLI run <game.exe> [--args A] [--depth] [--save-after SECONDS]
//
// run starts an exe with capture, hooked from its first instruction like a game launched
// from the library, without adding it there.
//
// Keys while capturing:  S = save current frame as BMP,  Q = quit.
// --depth also asks for the game's depth buffer (3D from depth) and says how many frames
// came with it; saving then also writes the frame's 3D pair. --save-after saves once, that
// many seconds in, and quits.

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
#include "gazeora/library/game_library_services.h"
#include "gazeora/library/platform/library_location.h"

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
             L"  %ls attach <pid | process.exe> [--depth] [--save-after SECONDS]\n"
             L"  %ls run <game.exe> [--args A] [--depth] [--save-after SECONDS]\n",
             exe, exe, exe, exe, exe, exe);
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

struct CaptureOptions {
    bool depth = false;
    double saveAfter = -1;  // seconds; negative: only on S
};

// Saves the current frame as frame_N.bmp, and with its depth the 3D pair as frame_N_3d.bmp:
// the two views side by side, as the phone gets them.
void SaveFrame(gazeora::CaptureClient& client) {
    std::wstring name = L"frame_" + std::to_wstring(client.Shared().frameIndex);
    std::wstring error;
    if (gazeora::SaveTextureAsBmp(client.Device(), client.Frame(), name + L".bmp", error)) {
        wprintf(L"Saved %ls.bmp\n", name.c_str());
    } else {
        fwprintf(stderr, L"Save failed: %ls\n", error.c_str());
    }
    if (!client.Depth()) {
        return;
    }
    gazeora::VrLayout vr;
    vr.enabled = vr.stereo = true;
    gazeora::TextureReader reader;
    reader.SetVrLayout(vr);
    std::vector<uint8_t> bgra;
    UINT width, height;
    // Twice: the first read learns how the game stores depth, as streaming does over its first
    // frame.
    reader.Read(client.Device(), client.Frame(), width, height, bgra, error, client.Depth());
    if (reader.Read(client.Device(), client.Frame(), width, height, bgra, error, client.Depth()) &&
        reader.Stereo() && gazeora::SaveBgraAsBmp(name + L"_3d.bmp", width, height, bgra, error)) {
        wprintf(L"Saved %ls_3d.bmp\n", name.c_str());
    } else {
        fwprintf(stderr, L"Saving the 3D pair failed: %ls\n", error.c_str());
    }
}

// Prints capture stats until Q / Ctrl+C, or until `game` (if given) exits.
int RunCaptureLoop(gazeora::CaptureClient& client, HANDLE game,
                   const CaptureOptions& options = {}) {
    client.SetCaptureEnabled(true);
    client.SetCaptureDepth(options.depth);
    SetConsoleCtrlHandler(OnConsoleCtrl, TRUE);
    printf("Press S to save a frame, Q to quit.\n");

    LARGE_INTEGER freq, statsStart, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&statsStart);
    LARGE_INTEGER started = statsStart;
    int frames = 0, depthFrames = 0;
    double handoffSum = 0.0;
    LONG lastState = -100;
    std::string lastStatus;
    std::wstring error;

    while (!g_quit) {
        bool gotFrame = client.WaitForFrame(100) == gazeora::CaptureClient::WaitResult::Frame;
        if (gotFrame) {
            ++frames;
            depthFrames += client.Depth() ? 1 : 0;
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
                printf("%5.1f fps  %ux%u %s  handoff %.2f ms  skipped %lld", frames / elapsed,
                       shared.width, shared.height, gazeora::GraphicsApiName(shared.api),
                       handoffSum / frames, shared.skippedFrames);
                if (options.depth) {
                    const gazeora::DepthFrame* depth = client.Depth();
                    printf("  depth %d/%d", depthFrames, frames);
                    if (depth) {
                        printf(" (%ux%u, %s)", depth->width, depth->height,
                               depth->encoding == gazeora::kDepthUnorm24 ? "24-bit" : "float");
                    }
                }
                printf("\n");
            }
            frames = depthFrames = 0;
            handoffSum = 0.0;
            statsStart = now;
        }

        while (_kbhit()) {
            int key = towupper(_getwch());
            if (key == L'Q') {
                g_quit = true;
            } else if (key == L'S') {
                SaveFrame(client);
            }
        }
        if (options.saveAfter >= 0 && gotFrame &&
            double(now.QuadPart - started.QuadPart) / double(freq.QuadPart) >= options.saveAfter) {
            SaveFrame(client);
            g_quit = true;
        }
    }

    // The hook stays loaded (it can't be safely unloaded); it just stops copying.
    client.SetCaptureEnabled(false);
    printf("Capture stopped.\n");
    return 0;
}

int CmdAttach(const std::wstring& target, const CaptureOptions& options) {
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
    return RunCaptureLoop(client, nullptr, options);
}

int CmdRun(const std::wstring& exe, const std::wstring& arguments, const CaptureOptions& options) {
    gazeora::LaunchSpec spec;
    spec.exePath = FullPath(exe);
    spec.arguments = arguments;
    gazeora::CaptureClient client;
    gazeora::LaunchedGame launched;
    std::wstring error;
    if (!gazeora::LaunchGameWithHook(spec, gazeora::HookDllPathNextToExe(), client, launched,
                                     error)) {
        fwprintf(stderr, L"launch failed: %ls\n", error.c_str());
        return 1;
    }
    wprintf(L"Launched %ls (PID %lu) with capture\n", spec.exePath.c_str(), launched.pid);
    return RunCaptureLoop(client, launched.process, options);
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
    gazeora::GameLibraryServices services(gazeora::LibraryLocation::DefaultPath());
    gazeora::GameLibrary& library = services.Library();
    if (!LoadLibraryOrReport(library)) {
        return 1;
    }
    if (library.Games().empty()) {
        wprintf(L"No games yet. Add one with: GazeoraCLI add <game.exe>\n");
        return 0;
    }
    wprintf(L"%4ls  %-32ls  %-9ls  %-8ls  %-14ls  %ls\n", L"ID", L"NAME", L"PLAYTIME", L"CAPTURE",
            L"LAUNCH", L"PATH");
    for (const gazeora::Game& game : library.Games()) {
        gazeora::LaunchSpec spec = services.LaunchSpecs().Create(game);
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
    gazeora::GameLibraryServices services(gazeora::LibraryLocation::DefaultPath());
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
        if (!services.Shortcuts().Resolve(path, resolved)) {
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

    gazeora::GameLibrary& library = services.Library();
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
    gazeora::GameLibraryServices services(gazeora::LibraryLocation::DefaultPath());
    gazeora::GameLibrary& library = services.Library();
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
    gazeora::GameLibraryServices services(gazeora::LibraryLocation::DefaultPath());
    gazeora::GameLibrary& library = services.Library();
    if (!LoadLibraryOrReport(library)) {
        return 1;
    }
    gazeora::Game* game = library.FindByIdOrName(key);
    if (!game) {
        fwprintf(stderr, L"no single game matches '%ls' (see GazeoraCLI list)\n", key.c_str());
        return 1;
    }
    gazeora::LaunchSpec spec = services.LaunchSpecs().Create(*game);
    bool capture = forceCapture || game->captureOnLaunch;
    uint32_t id = game->id;
    std::wstring name = game->name;
    bool throughSteam = !spec.steamAppId.empty();

    game->lastPlayed = services.Clock().NowUnixSeconds();
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
            if (!gazeora::WaitForGameProcess(
                    spec.GameProcessName(), ignorePid, gazeora::kWaitForGameMs, 30000,
                    [] { return bool(g_quit); }, launched, error)) {
                fwprintf(stderr, L"the game didn't start: %ls\n", error.c_str());
                return 1;
            }
            client = std::make_unique<gazeora::CaptureClient>();
            if (!gazeora::AttachHook(launched.pid, dllPath, *client, error)) {
                fwprintf(stderr, L"attaching capture failed: %ls\n", error.c_str());
                return 1;
            }
            wprintf(L"Attached to %ls (PID %lu)\n", spec.GameProcessName().c_str(), launched.pid);
        }

        gazeora::HandOffDetector handOff(launched.pid, launched.process);
        QueryPerformanceCounter(&start);
        result = RunCaptureLoop(*client, launched.process);
        QueryPerformanceCounter(&end);
        if (g_quit || launched.Running() || !handOff.HandedOff()) {
            break;
        }
        // Gone right after starting, without a window: it most likely restarted through its
        // launcher.
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
    } else if ((command == L"attach" || command == L"run") && !args.empty()) {
        CaptureOptions options;
        std::wstring arguments;
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == L"--depth") {
                options.depth = true;
            } else if (args[i] == L"--save-after" && i + 1 < args.size()) {
                options.saveAfter = _wtof(args[++i].c_str());
            } else if (command == L"run" && args[i] == L"--args" && i + 1 < args.size()) {
                arguments = args[++i];
            } else {
                PrintUsage(argv[0]);
                return 1;
            }
        }
        return command == L"run" ? CmdRun(args[0], arguments, options)
                                 : CmdAttach(args[0], options);
    } else if (command == L"help" || command == L"--help" || command == L"-h") {
        PrintUsage(argv[0]);
        return 0;
    } else if (args.empty() && command[0] != L'-') {
        // The original interface: GazeoraCLI <pid | process.exe>.
        return CmdAttach(command, {});
    }
    PrintUsage(argv[0]);
    return 1;
}
