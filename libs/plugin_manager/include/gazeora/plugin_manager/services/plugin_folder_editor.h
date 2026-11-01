// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/update/interfaces/file_hasher.h"

namespace gazeora {

// Installs and removes plugin DLLs in the plugins folder; run as an administrator. A running
// Gazeora keeps its plugins loaded, and a loaded DLL can't be deleted, so a replaced or removed
// DLL is renamed to *.removed, which no Gazeora loads, and deleted by a later CleanUp.
class PluginFolderEditor final {
public:
    PluginFolderEditor(std::wstring folder, const IFileHasher& hasher)
        : m_folder(std::move(folder)), m_hasher(hasher) {}

    // Copies `source` in as <id>.dll, replacing an installed one, if the copy's SHA-256 is
    // `sha256`: the copy is checked, not the source, which its owner could still change.
    // Throws PluginManagerError.
    void Install(const std::wstring& source, const std::wstring& id,
                 const std::string& sha256) const;
    // Removes the plugin DLL `fileName`, e.g. "example.dll". Throws PluginManagerError.
    void Uninstall(const std::wstring& fileName) const;
    // Deletes what earlier changes left behind and nothing has open any more.
    void CleanUp() const;

private:
    std::wstring PathOf(const std::wstring& fileName) const;
    // Moves `path` aside under a name that isn't loaded, and deletes it if it can.
    void Retire(const std::wstring& path) const;
    // Removes `staged`, then throws.
    [[noreturn]] void Abandon(const std::wstring& staged, const std::string& why) const;

    const std::wstring m_folder;
    const IFileHasher& m_hasher;
};

}  // namespace gazeora
