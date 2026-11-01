// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

namespace gazeora {

// The folder plugin DLLs are loaded from.
class PluginFolder final {
public:
    explicit PluginFolder(std::wstring path) : m_path(std::move(path)) {}
    // Where installed plugins load from: "plugins" beside the folder the running exe is in
    // (C:\Program Files\Gazeora\plugins for Gazeora in ...\Gazeora\bin). Only an
    // administrator can write there in an MSI install, which matters because Gazeora
    // sometimes runs as one.
    static PluginFolder Installed();
    // Where the bundled plugins wait until they're installed: "bundled-plugins" beside the
    // exe's folder (C:\Program Files\Gazeora\bundled-plugins). Gazeora never loads plugins
    // from here.
    static PluginFolder Bundled();

    const std::wstring& Path() const { return m_path; }
    // The DLLs in it, by name, so they load in the same order every time. Empty if the folder
    // doesn't exist.
    std::vector<std::wstring> Dlls() const;

private:
    const std::wstring m_path;
};

}  // namespace gazeora
