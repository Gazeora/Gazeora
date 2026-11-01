// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/ui_look.h"

#include <array>
#include <utility>

#include "ui/theme.h"

namespace gazeora::app {

namespace {

using ui::UiIcon;
using ui::UiTone;

const std::array<std::pair<UiTone, ImU32>, 7> kColors = {{
    {UiTone::Normal, kText},
    {UiTone::Dim, kTextDim},
    {UiTone::Faint, kTextFaint},
    {UiTone::Accent, kAccent},
    {UiTone::Success, kSuccess},
    {UiTone::Warning, kWarning},
    {UiTone::Danger, kDanger},
}};

// Every icon here is in the icon font's glyphs (kAllIcons).
constexpr std::array<std::pair<UiIcon, const char*>, 19> kGlyphs = {{
    {UiIcon::Add, ICON_ADD},           {UiIcon::Camera, ICON_CAMERA},
    {UiIcon::Check, ICON_CHECK},       {UiIcon::Chip, ICON_CHIP},
    {UiIcon::Clock, ICON_CLOCK},       {UiIcon::Delete, ICON_DELETE},
    {UiIcon::Edit, ICON_EDIT},         {UiIcon::Folder, ICON_FOLDER},
    {UiIcon::Game, ICON_GAME},         {UiIcon::Globe, ICON_GLOBE},
    {UiIcon::Hand, ICON_HAND},         {UiIcon::Info, ICON_INFO},
    {UiIcon::Play, ICON_PLAY},         {UiIcon::Plugin, ICON_PLUGIN},
    {UiIcon::Refresh, ICON_REFRESH},   {UiIcon::Search, ICON_SEARCH},
    {UiIcon::Settings, ICON_SETTINGS}, {UiIcon::Shop, ICON_SHOP},
    {UiIcon::Warning, ICON_WARNING},
}};

}  // namespace

ImU32 UiLook::Color(UiTone tone) {
    for (const auto& [known, color] : kColors) {
        if (known == tone) {
            return color;
        }
    }
    return kText;
}

const char* UiLook::Glyph(UiIcon icon) {
    for (const auto& [known, glyph] : kGlyphs) {
        if (known == icon) {
            return glyph;
        }
    }
    return "";
}

}  // namespace gazeora::app
