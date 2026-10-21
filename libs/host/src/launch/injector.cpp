#include "gazeora/host/launch/injector.h"

#include <tlhelp32.h>

namespace gazeora {

namespace {

std::wstring LastErrorString(const wchar_t* what) {
    return std::wstring(what) + L" failed (error " + std::to_wstring(GetLastError()) + L")";
}

struct HandleCloser {
    HANDLE h;
    ~HandleCloser() {
        if (h && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
    }
};

}  // namespace

std::wstring HookDllPathNextToExe() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path = exePath;
    path.resize(path.find_last_of(L"\\/") + 1);
    return path + L"gazeora_hook.dll";
}

DWORD FindProcessByName(const std::wstring& exeName, DWORD ignorePid) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    HandleCloser closer{snapshot};

    PROCESSENTRY32W entry = {sizeof(entry)};
    for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
        if (entry.th32ProcessID != ignorePid && _wcsicmp(entry.szExeFile, exeName.c_str()) == 0) {
            return entry.th32ProcessID;
        }
    }
    return 0;
}

bool InjectDll(DWORD pid, const std::wstring& dllPath, std::wstring& error) {
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                     PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process) {
        error = LastErrorString(L"OpenProcess");
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            error += L" - the game may be running as administrator or protected by anti-cheat";
        }
        return false;
    }
    HandleCloser processCloser{process};

    // The host and hook are 64-bit; LoadLibraryW's address only matches in a 64-bit target.
    BOOL isWow64 = FALSE;
    if (IsWow64Process(process, &isWow64) && isWow64) {
        error = L"target is a 32-bit process; only 64-bit games are supported";
        return false;
    }

    size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remotePath =
        VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        error = LastErrorString(L"VirtualAllocEx");
        return false;
    }

    bool ok = false;
    bool remoteThreadRunning = false;
    if (!WriteProcessMemory(process, remotePath, dllPath.c_str(), bytes, nullptr)) {
        error = LastErrorString(L"WriteProcessMemory");
    } else {
        // kernel32 is mapped at the same address in every process of a boot session.
        auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
        HANDLE thread =
            CreateRemoteThread(process, nullptr, 0, loadLibrary, remotePath, 0, nullptr);
        if (!thread) {
            error = LastErrorString(L"CreateRemoteThread");
        } else {
            HandleCloser threadCloser{thread};
            if (WaitForSingleObject(thread, 10000) != WAIT_OBJECT_0) {
                error = L"timed out waiting for LoadLibraryW in the target";
                remoteThreadRunning = true;
            } else {
                // Exit code is the low 32 bits of the HMODULE; 0 means LoadLibrary failed.
                DWORD exitCode = 0;
                GetExitCodeThread(thread, &exitCode);
                ok = exitCode != 0;
                if (!ok) {
                    error = L"LoadLibraryW failed inside the target (missing DLL dependency?)";
                }
            }
        }
    }

    // Leak the path rather than free it under a LoadLibraryW that may still be reading it.
    if (!remoteThreadRunning) {
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    }
    return ok;
}

}  // namespace gazeora
