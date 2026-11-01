// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/plugin_grid.h"

#include <algorithm>

namespace gazeora::app {

namespace {

constexpr float kMinTileWidth = 260.0f;
constexpr float kMinTileHeight = 180.0f;
constexpr float kGap = 14.0f;

}  // namespace

void PluginGrid::Draw(const PluginCard::Fonts& fonts,
                      const std::vector<const PluginEntry*>& entries, float width) {
    const int columns = ColumnsIn(width);
    const float tile = (width - S(kGap) * float(columns - 1)) / float(columns);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(kGap), S(kGap)));  // between the rows
    for (size_t first = 0; first < entries.size(); first += size_t(columns)) {
        const size_t count = std::min(size_t(columns), entries.size() - first);
        const float height = RowHeight(entries, first, count);
        for (size_t i = 0; i < count; ++i) {
            if (i > 0) {
                ImGui::SameLine(0.0f, S(kGap));
            }
            const PluginEntry& entry = *entries[first + i];
            m_heights[entry.id] = m_card.Draw(fonts, entry, ImVec2(tile, height));
        }
    }
    ImGui::PopStyleVar();
}

int PluginGrid::ColumnsIn(float width) const {
    return std::max(1, int((width + S(kGap)) / (S(kMinTileWidth) + S(kGap))));
}

float PluginGrid::RowHeight(const std::vector<const PluginEntry*>& entries, size_t first,
                            size_t count) const {
    float height = S(kMinTileHeight);
    for (size_t i = first; i < first + count; ++i) {
        auto measured = m_heights.find(entries[i]->id);
        if (measured != m_heights.end()) {
            height = std::max(height, measured->second);
        }
    }
    return height;
}

}  // namespace gazeora::app
