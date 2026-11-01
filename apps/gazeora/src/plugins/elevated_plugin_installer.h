// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include <string>

#include "plugins/plugin_installer.h"

namespace gazeora::app {

// Changes the plugins folder through Gazeora run as an administrator (PluginCommand): one
// Windows prompt per change. Blocks until it's done, so call it off the UI thread.
class ElevatedPluginInstaller final : public IPluginInstaller {
public:
    explicit ElevatedPluginInstaller(HWND owner) : m_owner(owner) {}

    bool Install(const std::wstring& source, const std::wstring& id, const std::string& sha256,
                 std::wstring& error) override;
    bool Uninstall(const std::wstring& fileName, std::wstring& error) override;

private:
    const HWND m_owner;
};

}  // namespace gazeora::app
