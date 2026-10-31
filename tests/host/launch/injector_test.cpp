// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/launch/injector.h"

#include <gtest/gtest.h>

#include <cwctype>
#include <string>

namespace gazeora {
namespace {

std::wstring OwnExeName() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full = path;
    return full.substr(full.find_last_of(L"\\/") + 1);
}

std::wstring SystemDll(const wchar_t* name) {
    wchar_t dir[MAX_PATH];
    GetSystemDirectoryW(dir, MAX_PATH);
    return std::wstring(dir) + L"\\" + name;
}

TEST(Injector, FindProcessByNameFindsRunningProcess) {
    // ctest may run several copies of this executable at once, so any of them will do.
    EXPECT_NE(FindProcessByName(OwnExeName()), 0u);
}

TEST(Injector, FindProcessByNameIgnoresCase) {
    std::wstring upper = OwnExeName();
    for (wchar_t& c : upper) c = wchar_t(towupper(c));
    EXPECT_NE(FindProcessByName(upper), 0u);
}

TEST(Injector, FindProcessByNameReturnsZeroWhenMissing) {
    EXPECT_EQ(FindProcessByName(L"gazeora_no_such_process_6f1c.exe"), 0u);
    EXPECT_EQ(FindProcessByName(L""), 0u);
}

TEST(Injector, InjectDllReportsOpenProcessFailure) {
    std::wstring error;
    EXPECT_FALSE(InjectDll(0, SystemDll(L"version.dll"), error));
    EXPECT_EQ(error.rfind(L"OpenProcess failed", 0), 0u);
}

TEST(Injector, InjectDllLoadsDllIntoTarget) {
    // Inject into ourselves, the one process the test is sure to have access to.
    const wchar_t* dll = L"d3d10.dll";
    if (GetModuleHandleW(dll)) GTEST_SKIP() << "already loaded";
    std::wstring error;
    ASSERT_TRUE(InjectDll(GetCurrentProcessId(), SystemDll(dll), error)) << error;
    HMODULE module = GetModuleHandleW(dll);
    EXPECT_NE(module, nullptr);
    FreeLibrary(module);
}

TEST(Injector, InjectDllRefusesAGameOfTheOtherBitness) {
    // Windows' own ping of the other bitness: SysWOW64's for a 64-bit build, the 64-bit one
    // (reached through Sysnative from a 32-bit process) for a 32-bit build.
    wchar_t windows[MAX_PATH];
    GetWindowsDirectoryW(windows, MAX_PATH);
    std::wstring ping = std::wstring(windows) +
                        (sizeof(void*) == 8 ? L"\\SysWOW64\\PING.EXE" : L"\\Sysnative\\PING.EXE");
    if (GetFileAttributesW(ping.c_str()) == INVALID_FILE_ATTRIBUTES) {
        GTEST_SKIP() << "no ping of the other bitness (32-bit Windows?)";
    }
    std::wstring commandLine = L"\"" + ping + L"\" -n 30 127.0.0.1";
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION target = {};
    ASSERT_TRUE(CreateProcessW(ping.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                               CREATE_NO_WINDOW, nullptr, nullptr, &startup, &target));
    std::wstring error;
    EXPECT_FALSE(InjectDll(target.dwProcessId, SystemDll(L"version.dll"), error));
    EXPECT_EQ(error, sizeof(void*) == 8
                         ? L"this is a 32-bit game: capture it with the x86 release of Gazeora"
                         : L"this is a 64-bit game: capture it with the x64 release of Gazeora");
    TerminateProcess(target.hProcess, 0);
    CloseHandle(target.hThread);
    CloseHandle(target.hProcess);
}

TEST(Injector, InjectDllReportsLoadLibraryFailure) {
    std::wstring error;
    EXPECT_FALSE(InjectDll(GetCurrentProcessId(), L"C:\\gazeora\\no_such.dll", error));
    EXPECT_EQ(error, L"LoadLibraryW failed inside the target (missing DLL dependency?)");
}

}  // namespace
}  // namespace gazeora
