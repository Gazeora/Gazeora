// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

#include "gazeora/plugin_manager/interfaces/plugin_index.h"
#include "gazeora/update/interfaces/http_client.h"

namespace gazeora {

// The plugin list for Windows on gazeora.github.io.
inline constexpr char kWindowsPluginIndex[] =
    "https://gazeora.github.io/data/plugins/windows/index.json";

// The plugin list as a JSON file on the web (docs/plugins.md describes it). Entries that are
// malformed, or whose downloads aren't https with a SHA-256, are left out.
class WebPluginIndex final : public IPluginIndex {
public:
    // `arch` picks the downloads: "x64" or "x86".
    WebPluginIndex(IHttpClient& http, std::string url, std::string arch)
        : m_http(http), m_url(std::move(url)), m_arch(std::move(arch)) {}

    // Throws PluginManagerError, or UpdateError for the download itself.
    std::vector<PluginListing> Fetch() override;

private:
    IHttpClient& m_http;
    const std::string m_url;
    const std::string m_arch;
};

}  // namespace gazeora
