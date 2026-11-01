// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_manager/models/plugin_category.h"

#include <array>
#include <utility>

namespace gazeora {

namespace {

// In the order that breaks ties.
constexpr std::array<std::pair<PluginCategory, const char*>, 6> kIds = {{
    {PluginCategory::Protocol, "protocol"},
    {PluginCategory::Codec, "codec"},
    {PluginCategory::Encoder, "encoder"},
    {PluginCategory::GameStore, "game-store"},
    {PluginCategory::Controller, "controller"},
    {PluginCategory::Other, "other"},
}};

}  // namespace

std::optional<PluginCategory> PluginCategories::Parse(const std::string& id) {
    for (const auto& [category, name] : kIds) {
        if (id == name) {
            return category;
        }
    }
    return std::nullopt;
}

const char* PluginCategories::IdOf(PluginCategory category) {
    for (const auto& [known, name] : kIds) {
        if (known == category) {
            return name;
        }
    }
    return "other";
}

PluginCategory PluginCategories::Of(const PluginStatus& plugin) {
    const std::array<std::pair<PluginCategory, int>, 5> counts = {{
        {PluginCategory::Protocol, plugin.protocols},
        {PluginCategory::Codec, plugin.codecs},
        {PluginCategory::Encoder, plugin.encoders},
        {PluginCategory::GameStore, plugin.gameStores},
        {PluginCategory::Controller, plugin.gamepadDrivers},
    }};
    std::pair<PluginCategory, int> most = {PluginCategory::Other, 0};
    for (const auto& count : counts) {
        if (count.second > most.second) {
            most = count;
        }
    }
    return most.first;
}

}  // namespace gazeora
