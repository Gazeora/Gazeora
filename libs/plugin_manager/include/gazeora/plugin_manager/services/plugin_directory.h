// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

#include "gazeora/plugin_manager/models/plugin_entry.h"

namespace gazeora {

// Matches the installed plugins with the plugin list, and searches the result.
class PluginDirectory final {
public:
    PluginDirectory() = delete;

    // One entry per plugin id: the installed ones first, then the rest of the list, each by
    // name. A later listing of an id replaces an earlier one.
    static std::vector<PluginEntry> Join(const std::vector<PluginStatus>& installed,
                                         const std::vector<PluginListing>& listed);
    // Whether `query` is in the entry's name, summary, author or id, ignoring case. An empty
    // query matches everything.
    static bool Matches(const PluginEntry& entry, const std::wstring& query);
};

}  // namespace gazeora
