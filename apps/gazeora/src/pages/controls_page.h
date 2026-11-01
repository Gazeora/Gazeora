// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include "pages/head_tracking_card.h"
#include "plugin_ui/plugin_panels_view.h"

namespace gazeora::app {

// The Controls page: ways to control Gazeora besides the PC's own mouse and keyboard, which
// always keep working. Head tracking, and plugins' cards such as the Game Controller plugin's.
class ControlsPage {
public:
    struct Fonts {
        ImFont* title;
        ImFont* heading;
        ImFont* caption;
    };

    ControlsPage(HeadTrackingCard& head, PluginPanelsView& plugins, float scale)
        : m_head(head), m_plugins(plugins), m_scale(scale) {}

    void Draw(const Fonts& fonts);

private:
    float S(float px) const { return px * m_scale; }

    HeadTrackingCard& m_head;
    PluginPanelsView& m_plugins;
    float m_scale;
};

}  // namespace gazeora::app
