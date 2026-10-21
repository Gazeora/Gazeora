#pragma once

#include <windows.h>
#include <string>

namespace gazeora {

// gazeora_hook.dll in the same folder as the running executable.
std::wstring HookDllPathNextToExe();

// Returns the PID of the first running process whose exe name matches (case-insensitive) and
// isn't `ignorePid`, or 0.
DWORD FindProcessByName(const std::wstring& exeName, DWORD ignorePid = 0);

// Loads dllPath into the target process via CreateRemoteThread + LoadLibraryW.
bool InjectDll(DWORD pid, const std::wstring& dllPath, std::wstring& error);

}  // namespace gazeora
