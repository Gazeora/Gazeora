// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <array>

#include "plugin_manager/models/plugin_category.h"

namespace gazeora::app {

// How the Plugins page shows each plugin category: its icon and names.
class PluginCategoryLook final {
public:
    PluginCategoryLook() = delete;

    // Every category, in the order the filter lists them.
    static constexpr std::array<PluginCategory, 6> kAll = {
        PluginCategory::Protocol,  PluginCategory::Codec,      PluginCategory::Encoder,
        PluginCategory::GameStore, PluginCategory::Controller, PluginCategory::Other};

    static const char* Icon(PluginCategory category);
    // For one plugin, e.g. "Protocol".
    static const char* Name(PluginCategory category);
    // For the filter, e.g. "Protocols".
    static const char* Plural(PluginCategory category);
};

}  // namespace gazeora::app
