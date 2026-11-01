// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/plugin_ui/interfaces/ui_element_visitor.h"
#include "gazeora/plugin_ui/models/ui_element.h"
#include "plugin_ui/ui_draw_context.h"
#include "plugin_ui/ui_field_drawer.h"

namespace gazeora::app {

// Visitor pattern: draws a plugin panel's elements with ImGui, in Gazeora's own controls, so
// they look like the rest of Gazeora and a game controller moves through them the same way.
class ImGuiUiRenderer final : public ui::IUiElementVisitor {
public:
    explicit ImGuiUiRenderer(const UiDrawContext& context)
        : m_context(context), m_fields(context) {}

    // Each element shown while its visible-if holds, and usable while its enabled-if does.
    void DrawAll(const ui::UiElements& elements);

    void Visit(const ui::UiText& text) override;
    void Visit(const ui::UiToggle& toggle) override;
    void Visit(const ui::UiButton& button) override;
    void Visit(const ui::UiSlider& slider) override { m_fields.Draw(slider); }
    void Visit(const ui::UiInput& input) override { m_fields.Draw(input); }
    void Visit(const ui::UiChoice& choice) override { m_fields.Draw(choice); }
    void Visit(const ui::UiGroup& group) override;
    void Visit(const ui::UiTable& table) override;
    void Visit(const ui::UiSeparator& separator) override;

private:
    // False if it's hidden.
    bool Draw(const ui::UiElement& element, int index);

    const UiDrawContext& m_context;
    UiFieldDrawer m_fields;
};

}  // namespace gazeora::app
