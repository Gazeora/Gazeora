// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include "pages/gamepad_card.h"

namespace gazeora::app {

// The Controls page: ways to control Gazeora besides the PC's own mouse and keyboard, which
// always keep working. Game controllers.
class ControlsPage {
public:
    struct Fonts {
        ImFont* title;
        ImFont* heading;
    };

    ControlsPage(GamepadCard& gamepad, float scale) : m_gamepad(gamepad), m_scale(scale) {}

    void Draw(const Fonts& fonts);

private:
    float S(float px) const { return px * m_scale; }

    GamepadCard& m_gamepad;
    float m_scale;
};

}  // namespace gazeora::app
