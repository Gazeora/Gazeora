// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/plugin_panels_view.h"

#include <algorithm>
#include <string>

#include "gazeora/plugin_ui/services/ui_bindings.h"
#include "plugin_ui/imgui_ui_renderer.h"
#include "plugin_ui/ui_look.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

// As Gazeora's own cards: as wide as the settings' cards at most, spaced as theirs are.
constexpr float kMaxCardWidth = 720.0f;
constexpr ImVec2 kCardItemSpacing(10.0f, 14.0f);
constexpr char kIdPrefix[] = "##";
constexpr char kIconGap[] = "  ";

}  // namespace

void PluginPanelsView::Draw(ui::UiPage page, ui::UiPosition position, const Fonts& fonts,
                            float width) {
    if (width <= 0.0f) {
        width = std::min(ImGui::GetContentRegionAvail().x, S(kMaxCardWidth));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(S(kCardItemSpacing.x), S(kCardItemSpacing.y)));
    for (const ui::UiPanel& panel : m_panels) {
        if (panel.layout.page == page && panel.layout.position == position) {
            DrawPanel(panel, fonts, width);
        }
    }
    ImGui::PopStyleVar();
}

void PluginPanelsView::DrawPanel(const ui::UiPanel& panel, const Fonts& fonts, float width) {
    ui::UiBindings values(m_gazeora, *panel.values);
    std::string title = UiLook::Glyph(panel.layout.icon);
    if (!title.empty()) {
        title += kIconGap;
    }
    title += panel.layout.title;
    BeginCard((kIdPrefix + panel.id).c_str(), m_scale, width);
    CardTitle(fonts.heading, title.c_str());
    ImGui::PushID(panel.id.c_str());
    const UiDrawContext context{values, m_inputs, fonts.caption, m_scale};
    ImGuiUiRenderer(context).DrawAll(panel.layout.children);
    ImGui::PopID();
    EndCard();
}

}  // namespace gazeora::app
