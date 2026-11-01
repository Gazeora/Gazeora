// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_ui/models/ui_choice.h"
#include "gazeora/plugin_ui/models/ui_input.h"
#include "gazeora/plugin_ui/models/ui_slider.h"
#include "plugin_ui/ui_draw_context.h"

namespace gazeora::app {

// Draws a layout's fields, each with its label (and help) above it, as Gazeora's settings
// do: sliders, text boxes and drop-down lists.
class UiFieldDrawer {
public:
    explicit UiFieldDrawer(const UiDrawContext& context) : m_context(context) {}

    void Draw(const ui::UiSlider& slider);
    void Draw(const ui::UiInput& input);
    void Draw(const ui::UiChoice& choice);

    // Numbers as values carry them: shortest form, '.' for decimals, whatever the locale.
    static std::string FormatNumber(float number);
    static float ParseNumber(const std::string& text, float fallback);

private:
    void Label(const std::string& label, const std::string& help);

    const UiDrawContext& m_context;
};

}  // namespace gazeora::app
