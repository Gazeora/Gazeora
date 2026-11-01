// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/ui_field_drawer.h"

#include <imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <charconv>

#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr char kFieldId[] = "##field";
constexpr float kHelpWidth = 380.0f;
constexpr size_t kNumberCapacity = 32;

}  // namespace

void UiFieldDrawer::Draw(const ui::UiSlider& slider) {
    Label(slider.label, slider.help);
    float value = std::clamp(ParseNumber(m_context.values.Get(slider.bind), slider.min), slider.min,
                             slider.max);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat(kFieldId, &value, slider.min, slider.max, slider.format.c_str())) {
        m_context.values.Set(slider.bind, FormatNumber(value));
    }
}

void UiFieldDrawer::Draw(const ui::UiInput& input) {
    Label(input.label, input.help);
    const ImGuiID id = ImGui::GetID(kFieldId);
    std::string& text = m_context.inputs.Begin(id, m_context.values.Get(input.bind));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint(kFieldId, input.hint.c_str(), &text)) {
        m_context.values.Set(input.bind, text);
    }
    m_context.inputs.End(id, ImGui::IsItemActive());
    TypeOnSelect();
}

void UiFieldDrawer::Draw(const ui::UiChoice& choice) {
    Label(choice.label, choice.help);
    const std::string current = m_context.values.Get(choice.bind);
    auto chosen = std::find_if(choice.options.begin(), choice.options.end(),
                               [&](const ui::UiOption& option) { return option.value == current; });
    const char* shown = chosen == choice.options.end() ? "" : chosen->label.c_str();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (!ImGui::BeginCombo(kFieldId, shown)) {
        return;
    }
    for (const ui::UiOption& option : choice.options) {
        ImGui::PushID(option.value.c_str());
        if (ImGui::Selectable(option.label.c_str(), option.value == current)) {
            m_context.values.Set(choice.bind, option.value);
        }
        ImGui::PopID();
    }
    ImGui::EndCombo();
}

std::string UiFieldDrawer::FormatNumber(float number) {
    char text[kNumberCapacity];
    const auto result = std::to_chars(text, text + sizeof(text), number);
    return std::string(text, result.ptr);
}

float UiFieldDrawer::ParseNumber(const std::string& text, float fallback) {
    float number = fallback;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
    return result.ec == std::errc() ? number : fallback;
}

void UiFieldDrawer::Label(const std::string& label, const std::string& help) {
    if (label.empty()) {
        return;
    }
    FieldLabel(m_context.caption, label.c_str());
    if (!help.empty()) {
        HelpMarker(help.c_str(), m_context.S(kHelpWidth));
    }
}

}  // namespace gazeora::app
