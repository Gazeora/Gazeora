// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>
#include <string>

namespace gazeora {

// gazeora_hook.dll in the same folder as the running executable.
std::wstring HookDllPathNextToExe();

// Returns the PID of the first running process whose exe name matches (case-insensitive) and
// isn't `ignorePid`, or 0.
DWORD FindProcessByName(const std::wstring& exeName, DWORD ignorePid = 0);

// Loads dllPath into the target process via CreateRemoteThread + LoadLibraryW. The target
// must have this process's bitness, like the hook: the x64 release captures 64-bit games,
// the x86 release 32-bit ones.
bool InjectDll(DWORD pid, const std::wstring& dllPath, std::wstring& error);

}  // namespace gazeora
