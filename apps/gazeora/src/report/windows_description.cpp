// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "report/windows_description.h"

#include <windows.h>

#include <string>

namespace gazeora::app {

namespace {

using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
using IsWow64Process2Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);

// GetVersionEx reports the version the app is manifested for; ntdll reports the real one.
std::string WindowsVersion() {
    OSVERSIONINFOW info{sizeof(info)};
    auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    if (!rtlGetVersion || rtlGetVersion(&info) != 0) {
        return "Windows (version unknown)";
    }
    return "Windows " + std::to_string(info.dwMajorVersion) + "." +
           std::to_string(info.dwMinorVersion) + "." + std::to_string(info.dwBuildNumber);
}

const char* MachineName(USHORT machine) {
    switch (machine) {
        case IMAGE_FILE_MACHINE_AMD64:
            return "x64";
        case IMAGE_FILE_MACHINE_ARM64:
            return "ARM64";
        case IMAGE_FILE_MACHINE_I386:
            return "x86";
        default:
            return "unknown";
    }
}

// The PC's own processor, even when Gazeora runs emulated on it.
const char* NativeMachine() {
    auto isWow64Process2 = reinterpret_cast<IsWow64Process2Fn>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process2"));
    USHORT process = 0;
    USHORT native = 0;
    if (isWow64Process2 && isWow64Process2(GetCurrentProcess(), &process, &native)) {
        return MachineName(native);
    }
    SYSTEM_INFO info{};
    GetNativeSystemInfo(&info);
    return info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x64" : "unknown";
}

constexpr const char* BuildMachine() {
#if defined(_M_ARM64) || defined(__aarch64__)
    return "ARM64";
#elif defined(_M_X64) || defined(__x86_64__)
    return "x64";
#else
    return "x86";
#endif
}

}  // namespace

std::string DescribeWindows() {
    return WindowsVersion() + ", " + NativeMachine() + " PC, " + BuildMachine() + " build";
}

}  // namespace gazeora::app
