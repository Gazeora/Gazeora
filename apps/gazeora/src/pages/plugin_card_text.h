// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_manager/models/plugin_entry.h"

namespace gazeora::app {

// The short lines on a plugin's tile, UTF-8.
class PluginCardText final {
public:
    PluginCardText() = delete;

    // The installed version, or the offered one if it isn't installed; empty if neither is known.
    static std::string Version(const PluginEntry& entry);
    // What it added and what's new, e.g. "Adds 1 protocol · Version 1.1.0 available"; empty if
    // there's nothing to say.
    static std::string Facts(const PluginEntry& entry);
    // "Adds 1 codec and 2 protocols"; empty if it added nothing.
    static std::string Contents(const PluginStatus& plugin);
};

}  // namespace gazeora::app
