// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/launch/injector.h"

#include <tlhelp32.h>

#include "gazeora/host/launch/remote_thread_injector.h"

namespace gazeora {

namespace {

constexpr wchar_t kHookDll[] = L"gazeora_hook.dll";

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

// The folder `path` is in, with its trailing separator.
std::wstring FolderOf(const std::wstring& path) {
    return path.substr(0, path.find_last_of(L"\\/") + 1);
}

// A 32-bit process on 64-bit Windows (WOW64).
bool Is32Bit(HANDLE process) {
    BOOL wow64 = FALSE;
    return IsWow64Process(process, &wow64) && wow64;
}

}  // namespace

std::wstring HookDllPathNextToExe() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    return FolderOf(exePath) + kHookDll;
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

    // Each release captures games of its own bitness: the hook has to match the game.
    if (Is32Bit(process) != Is32Bit(GetCurrentProcess())) {
        error = Is32Bit(process)
                    ? L"this is a 32-bit game: capture it with the x86 release of Gazeora"
                    : L"this is a 64-bit game: capture it with the x64 release of Gazeora";
        return false;
    }
    return RemoteThreadInjector().Inject(process, dllPath, error);
}

}  // namespace gazeora
