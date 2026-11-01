// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_manager/services/plugin_folder_editor.h"

#include <windows.h>

#include <filesystem>
#include <system_error>

#include "gazeora/common/text/utf8.h"
#include "plugin_manager/exceptions/plugin_manager_error.h"
#include "plugin_manager/models/plugin_id.h"

namespace gazeora {

namespace {

constexpr wchar_t kDll[] = L".dll";
constexpr wchar_t kStaged[] = L".new";
constexpr wchar_t kRemoved[] = L".removed";
constexpr wchar_t kLeftoverPatterns[][12] = {L"\\*.removed", L"\\*.new"};

bool Exists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

}  // namespace

void PluginFolderEditor::Install(const std::wstring& source, const std::wstring& id,
                                 const std::string& sha256) const {
    if (!PluginId::IsValid(id)) {
        throw PluginManagerError("Not a usable plugin name: " + Utf8::FromWide(id));
    }
    std::error_code error;
    std::filesystem::create_directories(m_folder, error);
    const std::wstring target = PathOf(id + kDll);
    const std::wstring staged = target + kStaged;
    if (!CopyFileW(source.c_str(), staged.c_str(), FALSE)) {
        throw PluginManagerError("Couldn't copy the plugin into " + Utf8::FromWide(m_folder));
    }
    if (m_hasher.Sha256(staged) != sha256) {
        Abandon(staged, "The plugin changed after it was checked, so it wasn't installed");
    }
    if (Exists(target)) {
        Retire(target);
    }
    if (!MoveFileExW(staged.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        Abandon(staged, "Couldn't put the plugin in place");
    }
}

void PluginFolderEditor::Uninstall(const std::wstring& fileName) const {
    if (!PluginId::IsSafeDllName(fileName)) {
        throw PluginManagerError("Not a plugin file name: " + Utf8::FromWide(fileName));
    }
    const std::wstring path = PathOf(fileName);
    if (!Exists(path)) {
        throw PluginManagerError("The plugin " + Utf8::FromWide(fileName) + " isn't installed");
    }
    Retire(path);
}

void PluginFolderEditor::CleanUp() const {
    for (const wchar_t* pattern : kLeftoverPatterns) {
        WIN32_FIND_DATAW found;
        HANDLE find = FindFirstFileW((m_folder + pattern).c_str(), &found);
        if (find == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            DeleteFileW(PathOf(found.cFileName).c_str());  // fails while it's loaded
        } while (FindNextFileW(find, &found));
        FindClose(find);
    }
}

std::wstring PluginFolderEditor::PathOf(const std::wstring& fileName) const {
    return m_folder + L"\\" + fileName;
}

void PluginFolderEditor::Retire(const std::wstring& path) const {
    const std::wstring aside = path + L"." + std::to_wstring(GetTickCount64()) + kRemoved;
    if (!MoveFileExW(path.c_str(), aside.c_str(), 0)) {
        throw PluginManagerError("Couldn't remove " + Utf8::FromWide(path));
    }
    DeleteFileW(aside.c_str());
}

void PluginFolderEditor::Abandon(const std::wstring& staged, const std::string& why) const {
    DeleteFileW(staged.c_str());
    throw PluginManagerError(why);
}

}  // namespace gazeora
