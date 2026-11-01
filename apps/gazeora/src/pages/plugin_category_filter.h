// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include <optional>
#include <vector>

#include "plugin_manager/models/plugin_entry.h"

namespace gazeora::app {

// The row of category chips on the Plugins page: All, then each category that has a plugin,
// with how many. Picking one shows only that category's plugins.
class PluginCategoryFilter {
public:
    explicit PluginCategoryFilter(float scale) : m_scale(scale) {}

    void Draw(ImFont* font, const std::vector<PluginEntry>& entries);
    bool Matches(const PluginEntry& entry) const;
    // Whether a category is picked.
    bool Active() const { return m_selected.has_value(); }

private:
    // One chip; true when it's clicked.
    bool Chip(ImFont* font, const char* icon, const char* label, int count, bool selected);
    float S(float px) const { return px * m_scale; }

    const float m_scale;
    std::optional<PluginCategory> m_selected;  // none: all
};

}  // namespace gazeora::app
