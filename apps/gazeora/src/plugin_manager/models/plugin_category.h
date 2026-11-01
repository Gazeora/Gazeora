// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <optional>
#include <string>

#include "gazeora/plugins/models/plugin_status.h"

namespace gazeora {

// What kind of plugin it is, for the Plugins page's labels and filter.
enum class PluginCategory { Protocol, Codec, Encoder, GameStore, Controller, Other };

// Plugin categories by their ids in the plugin list ("protocol", "codec", "encoder",
// "game-store", "controller", "other"), and worked out from what a plugin registered.
class PluginCategories final {
public:
    PluginCategories() = delete;

    // Nothing for an id that isn't one of them.
    static std::optional<PluginCategory> Parse(const std::string& id);
    static const char* IdOf(PluginCategory category);
    // What a loaded plugin registered most of; protocols, then codecs, encoders, game stores and
    // game controller drivers break a tie. Other if it registered none.
    static PluginCategory Of(const PluginStatus& plugin);
};

}  // namespace gazeora
