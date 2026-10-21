#include "gazeora/host/launch/launcher.h"

#include <shellapi.h>
#include <tlhelp32.h>

#include <cwctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#include "gazeora/host/launch/injector.h"

namespace gazeora {

namespace {

std::wstring LastErrorString(const wchar_t* what) {
    return std::wstring(what) + L" failed (error " + std::to_wstring(GetLastError()) + L")";
}

std::wstring FolderOf(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

std::wstring FileName(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    int chars = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
    std::wstring result(chars, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), &result[0], chars);
    return result;
}

// The quoted tokens of one line of a Steam .acf/.vdf file: "key"  "value".
std::vector<std::string> QuotedTokens(const std::string& line) {
    std::vector<std::string> tokens;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] != '"') {
            continue;
        }
        std::string token;
        for (++i; i < line.size() && line[i] != '"'; ++i) {
            if (line[i] == '\\' && i + 1 < line.size()) {
                ++i;  // \\ and \" escapes
            }
            token += line[i];
        }
        tokens.push_back(token);
    }
    return tokens;
}

bool HasGraphicsRuntime(DWORD pid) {
    static const wchar_t* const kRuntimes[] = {L"dxgi.dll",     L"d3d9.dll",   L"d3d11.dll",
                                               L"d3d12.dll",    L"d3d8.dll",   L"opengl32.dll",
                                               L"vulkan-1.dll"};
    HANDLE snapshot = INVALID_HANDLE_VALUE;
    // Fails with ERROR_BAD_LENGTH while the target is loading modules; just retry.
    for (int attempt = 0; attempt < 5 && snapshot == INVALID_HANDLE_VALUE; ++attempt) {
        snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    }
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }
    bool found = false;
    MODULEENTRY32W entry = {sizeof(entry)};
    for (BOOL ok = Module32FirstW(snapshot, &entry); ok && !found;
         ok = Module32NextW(snapshot, &entry)) {
        for (const wchar_t* runtime : kRuntimes) {
            found = found || _wcsicmp(entry.szModule, runtime) == 0;
        }
    }
    CloseHandle(snapshot);
    return found;
}

bool HasVisibleWindow(DWORD pid) {
    struct Search {
        DWORD pid;
        bool found;
    } search = {pid, false};
    EnumWindows(
        [](HWND hwnd, LPARAM param) -> BOOL {
            auto* s = reinterpret_cast<Search*>(param);
            DWORD owner = 0;
            GetWindowThreadProcessId(hwnd, &owner);
            if (owner == s->pid && IsWindowVisible(hwnd) && !GetWindow(hwnd, GW_OWNER)) {
                s->found = true;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.found;
}

// Whether the game is far enough along to inject into. Graphics DLLs alone aren't enough:
// statically imported ones are mapped the moment the process is created, and a remote
// thread started before the loader finishes initializing the process makes it fail with
// STATUS_DLL_INIT_FAILED. A visible window means start-up is done, and comes before the
// swap chain (which needs the window).
bool ReadyForHook(DWORD pid) { return HasVisibleWindow(pid) && HasGraphicsRuntime(pid); }

bool StartProcess(const LaunchSpec& spec, DWORD flags, PROCESS_INFORMATION& pi,
                  std::wstring& error) {
    if (GetFileAttributesW(spec.exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        error = L"game executable not found: " + spec.exePath;
        return false;
    }
    // CreateProcess may modify the command line buffer, so it can't be a const string.
    std::wstring commandLine = L"\"" + spec.exePath + L"\"";
    if (!spec.arguments.empty()) {
        commandLine += L" " + spec.arguments;
    }
    std::wstring workingDir = spec.workingDir.empty() ? FolderOf(spec.exePath) : spec.workingDir;

    STARTUPINFOW si = {sizeof(si)};
    if (!CreateProcessW(spec.exePath.c_str(), &commandLine[0], nullptr, nullptr, FALSE, flags,
                        nullptr, workingDir.empty() ? nullptr : workingDir.c_str(), &si, &pi)) {
        error = LastErrorString(L"CreateProcess");
        if (GetLastError() == ERROR_ELEVATION_REQUIRED) {
            error += L" - the game requires administrator rights; run Gazeora as administrator";
        }
        return false;
    }
    return true;
}

}  // namespace

std::wstring LaunchSpec::GameProcessName() const {
    return processName.empty() ? FileName(exePath) : processName;
}

std::wstring FindSteamAppId(const std::wstring& exePath) {
    // ...\steamapps\common\<installdir>\...\game.exe
    std::wstring lower = exePath;
    for (wchar_t& c : lower) {
        c = c == L'/' ? L'\\' : wchar_t(towlower(c));
    }
    const std::wstring marker = L"\\steamapps\\common\\";
    size_t at = lower.find(marker);
    if (at == std::wstring::npos) {
        return {};
    }
    std::wstring steamapps = exePath.substr(0, at + wcslen(L"\\steamapps"));
    size_t dirStart = at + marker.size();
    size_t dirEnd = lower.find(L'\\', dirStart);
    std::wstring installDir = exePath.substr(dirStart, dirEnd - dirStart);

    WIN32_FIND_DATAW found;
    HANDLE find = FindFirstFileW((steamapps + L"\\appmanifest_*.acf").c_str(), &found);
    if (find == INVALID_HANDLE_VALUE) {
        return {};
    }
    std::wstring result;
    do {
        std::ifstream file(std::filesystem::path(steamapps + L"\\" + found.cFileName));
        std::string line, appId, dir;
        while (std::getline(file, line)) {
            std::vector<std::string> tokens = QuotedTokens(line);
            if (tokens.size() == 2 && tokens[0] == "appid") {
                appId = tokens[1];
            } else if (tokens.size() == 2 && tokens[0] == "installdir") {
                dir = tokens[1];
            }
        }
        if (!appId.empty() && _wcsicmp(Utf8ToWide(dir).c_str(), installDir.c_str()) == 0) {
            result = Utf8ToWide(appId);
        }
    } while (result.empty() && FindNextFileW(find, &found));
    FindClose(find);
    return result;
}

bool LaunchThroughSteam(const std::wstring& appId, std::wstring& error) {
    std::wstring uri = L"steam://rungameid/" + appId;
    auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        error = L"could not start the game through Steam (is Steam installed?), error " +
                std::to_wstring(result);
        return false;
    }
    return true;
}

bool WaitForGameProcess(const std::wstring& exeName, DWORD ignorePid, DWORD timeoutMs,
                        DWORD readyTimeoutMs, const std::function<bool()>& cancelled,
                        LaunchedGame& game, std::wstring& error) {
    ULONGLONG start = GetTickCount64();
    for (;;) {
        if (cancelled && cancelled()) {
            error = L"cancelled";
            return false;
        }
        if (GetTickCount64() - start > timeoutMs) {
            error = L"timed out waiting for " + exeName + L" to start";
            return false;
        }
        DWORD pid = FindProcessByName(exeName, ignorePid);
        if (pid == 0) {
            Sleep(250);
            continue;
        }
        HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) {
            error = LastErrorString(L"OpenProcess") +
                    L" - the game may be running as administrator; run Gazeora as administrator";
            return false;
        }

        ULONGLONG found = GetTickCount64();
        bool exited = false;
        while (!ReadyForHook(pid) && GetTickCount64() - found < readyTimeoutMs) {
            if (cancelled && cancelled()) {
                CloseHandle(process);
                error = L"cancelled";
                return false;
            }
            if (WaitForSingleObject(process, 100) == WAIT_OBJECT_0) {
                exited = true;
                break;
            }
        }
        if (exited) {
            // Another launcher stage; keep waiting for the next one.
            CloseHandle(process);
            ignorePid = pid;
            continue;
        }
        game = LaunchedGame();
        game.pid = pid;
        game.process = process;
        return true;
    }
}

bool AttachHook(DWORD pid, const std::wstring& dllPath, CaptureClient& client,
                std::wstring& error) {
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        error = L"hook DLL not found at " + dllPath;
        return false;
    }
    if (!client.Create(pid, error)) {
        return false;
    }
    return client.HookAlreadyLoaded() || InjectDll(pid, dllPath, error);
}

LaunchedGame& LaunchedGame::operator=(LaunchedGame&& other) noexcept {
    if (this != &other) {
        if (process) {
            CloseHandle(process);
        }
        pid = std::exchange(other.pid, 0);
        process = std::exchange(other.process, nullptr);
    }
    return *this;
}

LaunchedGame::~LaunchedGame() {
    if (process) {
        CloseHandle(process);
    }
}

bool LaunchGame(const LaunchSpec& spec, LaunchedGame& game, std::wstring& error) {
    PROCESS_INFORMATION pi = {};
    if (!StartProcess(spec, 0, pi, error)) {
        return false;
    }
    CloseHandle(pi.hThread);
    game = LaunchedGame();
    game.pid = pi.dwProcessId;
    game.process = pi.hProcess;
    return true;
}

bool LaunchGameWithHook(const LaunchSpec& spec, const std::wstring& dllPath,
                        CaptureClient& client, LaunchedGame& game, std::wstring& error) {
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        error = L"hook DLL not found at " + dllPath;
        return false;
    }
    PROCESS_INFORMATION pi = {};
    if (!StartProcess(spec, CREATE_SUSPENDED, pi, error)) {
        return false;
    }

    bool ok = client.Create(pi.dwProcessId, error);
    if (ok) {
        // Lets the hook load graphics runtimes itself, since the game hasn't yet.
        client.SetHostFlags(kHostLaunchedGame);
        ok = InjectDll(pi.dwProcessId, dllPath, error);
    }
    if (ok) {
        // The hook installs its detours on its own thread; resume once that's done so
        // the game can't create its device before the hooks are in place.
        for (int waited = 0; waited < 10000; waited += 10) {
            LONG state = client.Shared().hookState;
            if (state == kHookInstalled || state == kHookCapturing || state == kHookError) {
                break;
            }
            Sleep(10);
        }
    }
    if (!ok) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    }

    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    game = LaunchedGame();
    game.pid = pi.dwProcessId;
    game.process = pi.hProcess;
    return true;
}

}  // namespace gazeora
