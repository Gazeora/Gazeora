// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include <string>

namespace gazeora {

// Writes the DLL's path into the target and starts LoadLibraryW there on a remote thread.
// Only for a target of this process's bitness: kernel32 sits at the same address in every
// process of a boot session, but the 32-bit and 64-bit kernel32 are different DLLs.
class RemoteThreadInjector {
public:
    // Loads `dllPath` into `process`, opened with the rights InjectDll asks for. False with
    // `error` set if it couldn't.
    bool Inject(HANDLE process, const std::wstring& dllPath, std::wstring& error);
};

}  // namespace gazeora
