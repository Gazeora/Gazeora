// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include "gamepad/gamepad_control.h"

namespace gazeora::app {

// The Controls page's game controller card: the switch, what's connected, and which button
// does what. The guide is written out, not left to a tooltip: a controller can't hover.
class GamepadCard {
public:
    GamepadCard(GamepadControl& control, float scale);

    void Draw(ImFont* heading, float width);

private:
    void DrawSwitch();
    void DrawGuide();
    void DrawStatus();
    float S(float px) const { return px * m_scale; }

    GamepadControl& m_control;
    float m_scale;
};

}  // namespace gazeora::app
