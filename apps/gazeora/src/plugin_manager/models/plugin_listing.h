// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "gazeora/update/models/installer_asset.h"
#include "plugin_manager/models/plugin_category.h"

namespace gazeora {

// One plugin users can install: from the plugin list on gazeora.github.io, or one that comes
// with Gazeora (BundledPluginIndex).
struct PluginListing {
    std::wstring id;  // PluginId::IsValid; it installs as <id>.dll
    std::wstring name;
    std::wstring summary;
    std::wstring author;
    std::wstring license;
    std::string homepage;  // https, or empty
    std::string version;   // e.g. "1.2.0"
    uint32_t apiVersion = 0;
    // What kind of plugin the list says it is; none if it doesn't say, or says something unknown.
    std::optional<PluginCategory> category;
    // The build for this release's architecture; none if the plugin has none.
    std::optional<InstallerAsset> download;
    // For a plugin that comes with Gazeora: the DLL it installs from. Empty for the others.
    std::wstring bundledFile;
};

}  // namespace gazeora
