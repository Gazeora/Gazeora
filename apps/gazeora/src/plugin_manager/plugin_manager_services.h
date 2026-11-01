// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugins/platform/plugin_folder.h"
#include "gazeora/update/models/release_version.h"
#include "gazeora/update/platform/bcrypt_file_hasher.h"
#include "gazeora/update/platform/winhttp_client.h"
#include "gazeora/update/services/installer_downloader.h"
#include "plugin_manager/platform/plugin_download_location.h"
#include "plugin_manager/repositories/bundled_plugin_index.h"
#include "plugin_manager/repositories/web_plugin_index.h"

namespace gazeora {

// Facade pattern: the plugin list and plugin downloads, wired to gazeora.github.io and to
// Windows, and the plugins that come with Gazeora. The launcher's composition root for the Plugins
// page.
class PluginManagerServices final {
public:
    // `current` is this build's version; `arch` its architecture (x64 or x86).
    PluginManagerServices(const ReleaseVersion& current, std::string arch)
        : m_http(L"Gazeora/" + Utf8::ToWide(current.Label())),
          m_index(m_http, kWindowsPluginIndex, std::move(arch)),
          m_bundled(PluginFolder::Bundled().Path()),
          m_downloader(m_http, m_hasher, PluginDownloadLocation::Folder()) {}
    PluginManagerServices(const PluginManagerServices&) = delete;
    PluginManagerServices& operator=(const PluginManagerServices&) = delete;

    IPluginIndex& Index() { return m_index; }
    IPluginIndex& Bundled() { return m_bundled; }
    const InstallerDownloader& Downloader() const { return m_downloader; }
    const IFileHasher& Hasher() const { return m_hasher; }

private:
    // Declared before the services, which hold references to them.
    WinHttpClient m_http;
    BcryptFileHasher m_hasher;
    WebPluginIndex m_index;
    BundledPluginIndex m_bundled;
    InstallerDownloader m_downloader;
};

}  // namespace gazeora
