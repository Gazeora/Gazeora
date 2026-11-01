// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/plugin_category_filter.h"

#include <algorithm>
#include <string>

#include "pages/plugin_category_look.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr float kChipHeight = 30.0f;
constexpr float kChipPadX = 14.0f;
constexpr float kChipGap = 8.0f;

}  // namespace

void PluginCategoryFilter::Draw(ImFont* font, const std::vector<PluginEntry>& entries) {
    if (Chip(font, nullptr, "All", int(entries.size()), !m_selected)) {
        m_selected.reset();
    }
    for (PluginCategory category : PluginCategoryLook::kAll) {
        const int count = int(std::count_if(entries.begin(), entries.end(), [&](const auto& e) {
            return e.Category() == category;
        }));
        if (count == 0 && m_selected != category) {
            continue;
        }
        ImGui::SameLine(0.0f, S(kChipGap));
        if (Chip(font, PluginCategoryLook::Icon(category), PluginCategoryLook::Plural(category),
                 count, m_selected == category)) {
            m_selected = category;
        }
    }
}

bool PluginCategoryFilter::Matches(const PluginEntry& entry) const {
    return !m_selected || entry.Category() == *m_selected;
}

bool PluginCategoryFilter::Chip(ImFont* font, const char* icon, const char* label, int count,
                                bool selected) {
    std::string text = (icon ? std::string(icon) + "  " : std::string()) + label + "  " +
                       std::to_string(count) + "##chip" + label;
    const char* end = LabelEnd(text.c_str());
    const float width =
        font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text.c_str(), end).x + S(kChipPadX) * 2;
    const ImVec2 size(width, S(kChipHeight));
    return selected ? FilledButton(text.c_str(), size, kAccentSoft, kAccentSoft, kAccentHover, font,
                                   size.y * 0.5f, true)
                    : FilledButton(text.c_str(), size, kSecondary, kSecondaryHover, kTextDim, font,
                                   size.y * 0.5f, true);
}

}  // namespace gazeora::app
