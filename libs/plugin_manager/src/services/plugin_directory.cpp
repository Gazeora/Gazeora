// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_manager/services/plugin_directory.h"

#include <algorithm>
#include <cwctype>
#include <map>

#include "gazeora/plugin_manager/models/plugin_id.h"

namespace gazeora {

namespace {

std::wstring Lower(std::wstring text) {
    for (wchar_t& c : text) {
        c = wchar_t(towlower(c));
    }
    return text;
}

bool Contains(const std::wstring& text, const std::wstring& lowerQuery) {
    return Lower(text).find(lowerQuery) != std::wstring::npos;
}

bool InstalledFirstThenByName(const PluginEntry& a, const PluginEntry& b) {
    if (a.installed.has_value() != b.installed.has_value()) {
        return a.installed.has_value();
    }
    return Lower(a.Name()) < Lower(b.Name());
}

}  // namespace

std::vector<PluginEntry> PluginDirectory::Join(const std::vector<PluginStatus>& installed,
                                               const std::vector<PluginListing>& listed) {
    std::map<std::wstring, PluginEntry> byId;
    for (const PluginStatus& status : installed) {
        PluginEntry& entry = byId[PluginId::OfPath(status.path)];
        entry.id = PluginId::OfPath(status.path);
        entry.installed = status;
    }
    for (const PluginListing& listing : listed) {
        PluginEntry& entry = byId[listing.id];
        entry.id = listing.id;
        entry.listed = listing;
    }
    std::vector<PluginEntry> entries;
    for (auto& [id, entry] : byId) {
        entries.push_back(std::move(entry));
    }
    std::sort(entries.begin(), entries.end(), InstalledFirstThenByName);
    return entries;
}

bool PluginDirectory::Matches(const PluginEntry& entry, const std::wstring& query) {
    const std::wstring lower = Lower(query);
    if (lower.empty() || Contains(entry.id, lower) || Contains(entry.Name(), lower)) {
        return true;
    }
    return entry.listed &&
           (Contains(entry.listed->summary, lower) || Contains(entry.listed->author, lower));
}

}  // namespace gazeora
