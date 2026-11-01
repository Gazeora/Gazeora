// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/plugin_card_text.h"

#include <vector>

#include "gazeora/common/text/utf8.h"

namespace gazeora::app {

namespace {

constexpr char kSeparator[] = "  \xC2\xB7  ";

void Append(std::string& text, const std::string& part) {
    text += (text.empty() ? "" : kSeparator) + part;
}

}  // namespace

std::string PluginCardText::Version(const PluginEntry& entry) {
    if (entry.installed && !entry.installed->version.empty()) {
        return Utf8::FromWide(entry.installed->version);
    }
    return entry.listed ? entry.listed->version : std::string();
}

std::string PluginCardText::Facts(const PluginEntry& entry) {
    std::string facts;
    if (entry.installed && entry.installed->loaded) {
        facts = Contents(*entry.installed);
    }
    if (entry.UpdateAvailable()) {
        Append(facts, "Version " + entry.listed->version + " available");
    }
    if (entry.listed && !entry.listed->author.empty() && !entry.Bundled()) {
        Append(facts, "By " + Utf8::FromWide(entry.listed->author));
    }
    return facts;
}

std::string PluginCardText::Contents(const PluginStatus& plugin) {
    std::vector<std::string> parts;
    auto add = [&](int count, const char* one, const char* many) {
        if (count > 0) parts.push_back(std::to_string(count) + " " + (count == 1 ? one : many));
    };
    add(plugin.protocols, "protocol", "protocols");
    add(plugin.codecs, "codec", "codecs");
    add(plugin.encoders, "encoder", "encoders");
    add(plugin.gameStores, "game store", "game stores");
    add(plugin.gamepadDrivers, "game controller driver", "game controller drivers");
    add(plugin.uiPanels, "card", "cards");
    std::string text;
    for (size_t i = 0; i < parts.size(); ++i) {
        text += (i == 0 ? "Adds " : i + 1 == parts.size() ? " and " : ", ") + parts[i];
    }
    return text;
}

}  // namespace gazeora::app
