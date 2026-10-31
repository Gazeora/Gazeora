// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include "pages/gamepad_card.h"
#include "pages/head_tracking_card.h"

namespace gazeora::app {

// The Controls page: ways to control Gazeora besides the PC's own mouse and keyboard, which
// always keep working. Game controllers and head tracking.
class ControlsPage {
public:
    struct Fonts {
        ImFont* title;
        ImFont* heading;
        ImFont* caption;
    };

    ControlsPage(GamepadCard& gamepad, HeadTrackingCard& head, float scale)
        : m_gamepad(gamepad), m_head(head), m_scale(scale) {}

    void Draw(const Fonts& fonts);

private:
    float S(float px) const { return px * m_scale; }

    GamepadCard& m_gamepad;
    HeadTrackingCard& m_head;
    float m_scale;
};

}  // namespace gazeora::app
