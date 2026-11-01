// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include <vector>

#include "gazeora/plugin_ui/interfaces/ui_values.h"
#include "gazeora/plugin_ui/models/ui_panel.h"
#include "plugin_ui/ui_input_buffers.h"

namespace gazeora::app {

// The cards plugins add to Gazeora's pages, each drawn from its layout with what it binds to:
// Gazeora's own values (`gazeora`) and its plugin's. Each page calls Draw where its plugin
// cards go: under its title (Start) and after its own content (End).
class PluginPanelsView {
public:
    struct Fonts {
        ImFont* heading;  // card titles
        ImFont* caption;  // field labels
    };

    // `panels` and `gazeora` must outlive this.
    PluginPanelsView(const std::vector<ui::UiPanel>& panels, ui::IUiValues& gazeora, float scale)
        : m_panels(panels), m_gazeora(gazeora), m_scale(scale) {}

    // The panels for `page` at `position`, `width` wide (all the room left if 0).
    void Draw(ui::UiPage page, ui::UiPosition position, const Fonts& fonts, float width = 0.0f);

private:
    void DrawPanel(const ui::UiPanel& panel, const Fonts& fonts, float width);
    float S(float px) const { return px * m_scale; }

    const std::vector<ui::UiPanel>& m_panels;
    ui::IUiValues& m_gazeora;
    float m_scale;
    UiInputBuffers m_inputs;
};

}  // namespace gazeora::app
