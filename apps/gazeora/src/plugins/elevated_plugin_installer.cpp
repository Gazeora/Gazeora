// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugins/elevated_plugin_installer.h"

#include "gazeora/common/text/utf8.h"
#include "platform/elevated_run.h"
#include "plugins/plugin_command.h"

namespace gazeora::app {

bool ElevatedPluginInstaller::Install(const std::wstring& source, const std::wstring& id,
                                      const std::string& sha256, std::wstring& error) {
    return ElevatedRun::Run(m_owner, kInstallPluginArgument, {id, source, Utf8::ToWide(sha256)},
                            L"Installing the plugin", error);
}

bool ElevatedPluginInstaller::Uninstall(const std::wstring& fileName, std::wstring& error) {
    return ElevatedRun::Run(m_owner, kUninstallPluginArgument, {fileName}, L"Removing the plugin",
                            error);
}

}  // namespace gazeora::app
