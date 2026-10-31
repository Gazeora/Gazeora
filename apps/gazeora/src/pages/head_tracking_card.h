// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include "head/head_tracking_control.h"

namespace gazeora::app {

// The Controls page's head tracking card: the switch, the sensitivity, and whether the phone's
// head turns are reaching a game.
class HeadTrackingCard {
public:
    HeadTrackingCard(HeadTrackingControl& control, float scale)
        : m_control(control), m_scale(scale) {}

    void Draw(ImFont* heading, ImFont* caption, float width);

private:
    void DrawSwitch();
    void DrawSensitivity(ImFont* caption);
    void DrawStatus();
    float S(float px) const { return px * m_scale; }

    HeadTrackingControl& m_control;
    float m_scale;
};

}  // namespace gazeora::app
