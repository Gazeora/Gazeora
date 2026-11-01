// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/plugin_category_look.h"

#include "ui/theme.h"

namespace gazeora::app {

const char* PluginCategoryLook::Icon(PluginCategory category) {
    switch (category) {
        case PluginCategory::Protocol:
            return ICON_GLOBE;
        case PluginCategory::Codec:
            return ICON_CAPTURE;  // a video camera
        case PluginCategory::Encoder:
            return ICON_CHIP;
        case PluginCategory::GameStore:
            return ICON_SHOP;
        default:
            return ICON_PLUGIN;
    }
}

const char* PluginCategoryLook::Name(PluginCategory category) {
    switch (category) {
        case PluginCategory::Protocol:
            return "Protocol";
        case PluginCategory::Codec:
            return "Codec";
        case PluginCategory::Encoder:
            return "Encoder";
        case PluginCategory::GameStore:
            return "Game store";
        default:
            return "Other";
    }
}

const char* PluginCategoryLook::Plural(PluginCategory category) {
    switch (category) {
        case PluginCategory::Protocol:
            return "Protocols";
        case PluginCategory::Codec:
            return "Codecs";
        case PluginCategory::Encoder:
            return "Encoders";
        case PluginCategory::GameStore:
            return "Game stores";
        default:
            return "Other";
    }
}

}  // namespace gazeora::app
