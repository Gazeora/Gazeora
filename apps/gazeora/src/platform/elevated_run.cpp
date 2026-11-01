// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "platform/elevated_run.h"

#include <shellapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <cwchar>

#include "platform/win32_error_text.h"

namespace gazeora::app {

namespace {

// What the elevated run writes back of what went wrong, at most: the pipe it goes through holds
// more, so the write never waits on the reader (who reads once it has exited).
constexpr size_t kMaxResultChars = 1024;
constexpr DWORD kResultPipeBytes = 16384;

}  // namespace

bool ElevatedRun::Run(HWND owner, const wchar_t* command,
                      const std::vector<std::wstring>& arguments, const wchar_t* what,
                      std::wstring& error) {
    wchar_t exe[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, exe, ARRAYSIZE(exe));
    HANDLE readEnd = nullptr, writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, nullptr, kResultPipeBytes)) {
        error = Win32ErrorText(L"Couldn't create a pipe for the result");
        return false;
    }
    std::wstring parameters = std::wstring(command) + L" " +
                              std::to_wstring(uint64_t(reinterpret_cast<uintptr_t>(writeEnd))) +
                              Quoted(arguments);

    SHELLEXECUTEINFOW info = {sizeof(info)};
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.hwnd = owner;
    info.lpVerb = L"runas";  // as an administrator: Windows asks first
    info.lpFile = exe;
    info.lpParameters = parameters.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) {
        error = GetLastError() == ERROR_CANCELLED
                    ? std::wstring(what) + L" was cancelled: it needs administrator permission"
                    : Win32ErrorText(L"Couldn't get administrator permission");
        CloseHandle(readEnd);
        CloseHandle(writeEnd);
        return false;
    }
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    CloseHandle(writeEnd);
    if (code == 0) {
        CloseHandle(readEnd);
        return true;
    }
    error = ReadPipe(readEnd);
    CloseHandle(readEnd);
    if (error.empty()) {
        error = std::wstring(what) + L" failed";
    }
    return false;
}

std::wstring ElevatedRun::Quoted(const std::vector<std::wstring>& arguments) {
    std::wstring quoted;
    for (const std::wstring& argument : arguments) {
        quoted += L" \"" + argument + L"\"";
    }
    return quoted;
}

std::wstring ElevatedRun::ReadPipe(HANDLE pipe) {
    wchar_t buffer[kMaxResultChars] = {};
    DWORD available = 0, read = 0;
    if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || available == 0 ||
        !ReadFile(pipe, buffer, std::min<DWORD>(available, sizeof(buffer)), &read, nullptr)) {
        return {};
    }
    return std::wstring(buffer, read / sizeof(wchar_t));
}

DWORD ElevatedRun::ParentProcessId() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD self = GetCurrentProcessId(), parent = 0;
    PROCESSENTRY32W entry = {sizeof(entry)};
    for (BOOL ok = Process32FirstW(snapshot, &entry); ok && !parent;
         ok = Process32NextW(snapshot, &entry)) {
        if (entry.th32ProcessID == self) {
            parent = entry.th32ParentProcessID;
        }
    }
    CloseHandle(snapshot);
    return parent;
}

void ElevatedRun::ReportToParent(const std::wstring& pipe, const std::wstring& text) {
    wchar_t* end = nullptr;
    unsigned long long value = wcstoull(pipe.c_str(), &end, 10);
    DWORD parentId = ParentProcessId();
    if (pipe.empty() || *end || value == 0 || parentId == 0) {
        return;
    }
    HANDLE parent = OpenProcess(PROCESS_DUP_HANDLE, FALSE, parentId);
    if (!parent) {
        return;
    }
    HANDLE handle = nullptr;
    if (DuplicateHandle(parent, reinterpret_cast<HANDLE>(uintptr_t(value)), GetCurrentProcess(),
                        &handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        if (GetFileType(handle) == FILE_TYPE_PIPE) {
            DWORD written = 0;
            size_t chars = std::min(text.size(), kMaxResultChars);
            WriteFile(handle, text.data(), DWORD(chars * sizeof(wchar_t)), &written, nullptr);
        }
        CloseHandle(handle);
    }
    CloseHandle(parent);
}

}  // namespace gazeora::app
