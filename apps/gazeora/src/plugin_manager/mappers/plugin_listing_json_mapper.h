// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

#include "plugin_manager/models/plugin_listing.h"

namespace gazeora {

// The plugin list's JSON into PluginListings, for one architecture.
class PluginListingJsonMapper final {
public:
    explicit PluginListingJsonMapper(std::string arch) : m_arch(std::move(arch)) {}

    // Throws PluginManagerError if `text` isn't a plugin list at all.
    std::vector<PluginListing> FromIndex(const std::string& text) const;

private:
    // Nothing if the entry is malformed.
    std::optional<PluginListing> FromEntry(const nlohmann::json& entry) const;
    std::optional<InstallerAsset> DownloadOf(const nlohmann::json& entry,
                                             const std::wstring& id) const;

    const std::string m_arch;
};

}  // namespace gazeora
