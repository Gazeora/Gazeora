// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugins/platform/win32_plugin_module_loader.h"

#include <windows.h>

#include "gazeora/plugins/platform/win32_plugin_module.h"

namespace gazeora {

namespace {

constexpr DWORD kSearchFlags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;

const wchar_t* ThisArchitecture() { return sizeof(void*) == 8 ? L"x64" : L"x86"; }

}  // namespace

std::shared_ptr<const IPluginModule> Win32PluginModuleLoader::Load(const std::wstring& path,
                                                                   std::wstring& error) const {
    HMODULE module = LoadLibraryExW(path.c_str(), nullptr, kSearchFlags);
    if (module) {
        return std::make_shared<Win32PluginModule>(module);
    }
    const DWORD code = GetLastError();
    error = code == ERROR_BAD_EXE_FORMAT
                ? std::wstring(L"not built for this release's architecture (") +
                      ThisArchitecture() + L")"
                : L"could not load the DLL (error " + std::to_wstring(code) + L")";
    return nullptr;
}

}  // namespace gazeora
