// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/imgui_ui_renderer.h"

#include <optional>
#include <string>

#include "gazeora/plugin_ui/models/ui_button.h"
#include "gazeora/plugin_ui/models/ui_group.h"
#include "gazeora/plugin_ui/models/ui_separator.h"
#include "gazeora/plugin_ui/models/ui_table.h"
#include "gazeora/plugin_ui/models/ui_text.h"
#include "gazeora/plugin_ui/models/ui_toggle.h"
#include "plugin_ui/ui_look.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr float kHelpWidth = 380.0f;
constexpr char kToggleId[] = "##toggle";
constexpr char kButtonId[] = "##button";
constexpr char kTableId[] = "##table";
constexpr char kOn[] = "1";
constexpr char kOff[] = "0";
constexpr char kIconGap[] = "  ";

// `wrap`: at the card's edge; table cells don't, so their columns fit them.
void Colored(ImU32 color, const std::string& text, bool wrap) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushTextWrapPos(wrap ? 0.0f : -1.0f);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

}  // namespace

void ImGuiUiRenderer::DrawAll(const ui::UiElements& elements) {
    for (size_t i = 0; i < elements.size(); ++i) {
        Draw(*elements[i], int(i));
    }
}

bool ImGuiUiRenderer::Draw(const ui::UiElement& element, int index) {
    if (!element.visibleIf.Holds(m_context.values)) {
        return false;
    }
    ImGui::PushID(index);
    ImGui::BeginDisabled(!element.enabledIf.Holds(m_context.values));
    element.Accept(*this);
    ImGui::EndDisabled();
    ImGui::PopID();
    return true;
}

void ImGuiUiRenderer::Visit(const ui::UiText& text) {
    std::optional<ui::UiTone> tone = text.tone;
    if (!text.toneBind.empty()) {
        tone = ui::UiTones::Parse(m_context.values.Get(text.toneBind));
    }
    const std::string shown = text.bind.empty() ? text.text : m_context.values.Get(text.bind);
    Colored(UiLook::Color(tone.value_or(ui::UiTone::Normal)), shown, true);
}

void ImGuiUiRenderer::Visit(const ui::UiToggle& toggle) {
    ImGui::AlignTextToFramePadding();
    bool on = ui::UiCondition::IsTrue(m_context.values.Get(toggle.bind));
    if (Toggle(kToggleId, &on, m_context.scale)) {
        m_context.values.Set(toggle.bind, on ? kOn : kOff);
    }
    if (!toggle.label.empty()) {
        ImGui::SameLine();
        ImGui::TextUnformatted(toggle.label.c_str());
    }
    if (!toggle.help.empty()) {
        HelpMarker(toggle.help.c_str(), m_context.S(kHelpWidth));
    }
}

void ImGuiUiRenderer::Visit(const ui::UiButton& button) {
    std::string label = UiLook::Glyph(button.icon);
    if (!label.empty() && !button.label.empty()) {
        label += kIconGap;
    }
    label += button.label + kButtonId;
    if (ImGui::Button(label.c_str())) {
        m_context.values.Act(button.action);
    }
}

void ImGuiUiRenderer::Visit(const ui::UiGroup& group) {
    bool first = true;
    for (size_t i = 0; i < group.children.size(); ++i) {
        if (group.row && !first && group.children[i]->visibleIf.Holds(m_context.values)) {
            ImGui::SameLine();
        }
        first = !Draw(*group.children[i], int(i)) && first;
    }
}

void ImGuiUiRenderer::Visit(const ui::UiTable& table) {
    if (!ImGui::BeginTable(kTableId, table.columns, ImGuiTableFlags_SizingFixedFit)) {
        return;
    }
    for (const std::vector<ui::UiCell>& row : table.rows) {
        ImGui::TableNextRow();
        for (const ui::UiCell& cell : row) {
            ImGui::TableNextColumn();
            Colored(UiLook::Color(cell.tone), cell.text, false);
        }
    }
    ImGui::EndTable();
}

void ImGuiUiRenderer::Visit(const ui::UiSeparator&) { ImGui::Separator(); }

}  // namespace gazeora::app
