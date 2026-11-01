// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// The ids a layout names pages, tones and icons by.

#include <array>
#include <utility>

#include "gazeora/plugin_ui/models/ui_icon.h"
#include "gazeora/plugin_ui/models/ui_page.h"
#include "gazeora/plugin_ui/models/ui_tone.h"

namespace gazeora::ui {

namespace {

constexpr std::array<std::pair<UiPage, const char*>, 5> kPages = {{
    {UiPage::Home, "home"},
    {UiPage::Library, "library"},
    {UiPage::Controls, "controls"},
    {UiPage::Plugins, "plugins"},
    {UiPage::Settings, "settings"},
}};

constexpr std::array<std::pair<UiTone, const char*>, 7> kTones = {{
    {UiTone::Normal, "normal"},
    {UiTone::Dim, "dim"},
    {UiTone::Faint, "faint"},
    {UiTone::Accent, "accent"},
    {UiTone::Success, "success"},
    {UiTone::Warning, "warning"},
    {UiTone::Danger, "danger"},
}};

constexpr std::array<std::pair<UiIcon, const char*>, 20> kIcons = {{
    {UiIcon::None, ""},           {UiIcon::Add, "add"},         {UiIcon::Camera, "camera"},
    {UiIcon::Check, "check"},     {UiIcon::Chip, "chip"},       {UiIcon::Clock, "clock"},
    {UiIcon::Delete, "delete"},   {UiIcon::Edit, "edit"},       {UiIcon::Folder, "folder"},
    {UiIcon::Game, "game"},       {UiIcon::Globe, "globe"},     {UiIcon::Hand, "hand"},
    {UiIcon::Info, "info"},       {UiIcon::Play, "play"},       {UiIcon::Plugin, "plugin"},
    {UiIcon::Refresh, "refresh"}, {UiIcon::Search, "search"},   {UiIcon::Settings, "settings"},
    {UiIcon::Shop, "shop"},       {UiIcon::Warning, "warning"},
}};

template <typename Value, size_t Count>
std::optional<Value> Find(const std::array<std::pair<Value, const char*>, Count>& ids,
                          const std::string& id) {
    for (const auto& [value, name] : ids) {
        if (id == name) {
            return value;
        }
    }
    return std::nullopt;
}

}  // namespace

std::optional<UiPage> UiPages::Parse(const std::string& id) { return Find(kPages, id); }

std::optional<UiTone> UiTones::Parse(const std::string& id) { return Find(kTones, id); }

const char* UiTones::IdOf(UiTone tone) {
    for (const auto& [known, name] : kTones) {
        if (known == tone) {
            return name;
        }
    }
    return "normal";
}

std::optional<UiIcon> UiIcons::Parse(const std::string& name) { return Find(kIcons, name); }

}  // namespace gazeora::ui
