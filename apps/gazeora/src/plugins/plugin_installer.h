// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora::app {

// Changes the plugins folder. Changes take effect the next time Gazeora starts.
class IPluginInstaller {
public:
    virtual ~IPluginInstaller() = default;

    // Installs the DLL at `source` as <id>.dll if its SHA-256 is `sha256`. False with `error`
    // set if it couldn't.
    virtual bool Install(const std::wstring& source, const std::wstring& id,
                         const std::string& sha256, std::wstring& error) = 0;
    // Removes the plugin DLL `fileName`, e.g. "example.dll".
    virtual bool Uninstall(const std::wstring& fileName, std::wstring& error) = 0;
};

}  // namespace gazeora::app
