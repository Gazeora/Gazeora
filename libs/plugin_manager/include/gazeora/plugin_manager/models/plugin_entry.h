// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <optional>
#include <string>

#include "gazeora/plugin_manager/models/plugin_listing.h"
#include "gazeora/plugins/models/plugin_status.h"

namespace gazeora {

// One plugin on the Plugins page: installed, in the plugin list, or both.
struct PluginEntry {
    std::wstring id;
    std::optional<PluginStatus> installed;  // as it loaded when Gazeora started
    std::optional<PluginListing> listed;

    // The list's name if it has one, the plugin's own otherwise.
    std::wstring Name() const;
    // Whether the list has a build this Gazeora can install: its architecture, and a plugin
    // API version it loads. A bundled plugin always has one.
    bool Installable() const;
    // What kind of plugin it is: the list's word for it, else what it registered when it
    // loaded, else Other.
    PluginCategory Category() const;
    // Whether it comes with Gazeora (PluginListing::bundledFile).
    bool Bundled() const { return listed && !listed->bundledFile.empty(); }
    // Whether the list has a newer version than the one installed. False if either version
    // can't be read.
    bool UpdateAvailable() const;
};

}  // namespace gazeora
