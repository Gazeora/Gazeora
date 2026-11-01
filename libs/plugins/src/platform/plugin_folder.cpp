// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugins/platform/plugin_folder.h"

#include <windows.h>

#include <algorithm>

namespace gazeora {

namespace {

constexpr wchar_t kInstalledFolder[] = L"\\plugins";
constexpr wchar_t kBundledFolder[] = L"\\bundled-plugins";
constexpr wchar_t kDllPattern[] = L"\\*.dll";
constexpr wchar_t kDllExtension[] = L".dll";
constexpr size_t kDllExtensionLength = 4;
constexpr DWORD kLongPath = 32768;

// The pattern also matches 8.3 short names, so "a.dllx" could come back for "*.dll".
bool EndsInDll(const std::wstring& name) {
    return name.size() > kDllExtensionLength &&
           _wcsicmp(name.c_str() + name.size() - kDllExtensionLength, kDllExtension) == 0;
}

std::wstring Parent(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

// The folder above the exe's: the install folder, for an exe in its bin folder.
std::wstring RootFolder() {
    std::wstring path(kLongPath, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, &path[0], kLongPath);
    path.resize(length);
    return Parent(Parent(path));
}

}  // namespace

PluginFolder PluginFolder::Installed() { return PluginFolder(RootFolder() + kInstalledFolder); }

PluginFolder PluginFolder::Bundled() { return PluginFolder(RootFolder() + kBundledFolder); }

std::vector<std::wstring> PluginFolder::Dlls() const {
    std::vector<std::wstring> dlls;
    WIN32_FIND_DATAW found;
    HANDLE find = FindFirstFileW((m_path + kDllPattern).c_str(), &found);
    if (find == INVALID_HANDLE_VALUE) {
        return dlls;
    }
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && EndsInDll(found.cFileName)) {
            dlls.push_back(m_path + L"\\" + found.cFileName);
        }
    } while (FindNextFileW(find, &found));
    FindClose(find);
    std::sort(dlls.begin(), dlls.end());
    return dlls;
}

}  // namespace gazeora
