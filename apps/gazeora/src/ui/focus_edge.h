// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::app {

// How the control a controller is on stands out: a thin edge in the theme's violet-to-cyan
// gradient, as on the Play button, drawn just inside the control so it never runs over the
// ones next to it. ImGui's own box is turned off.
class FocusEdge {
public:
    explicit FocusEdge(float scale) : m_scale(scale) {}

    // Once a frame, after everything is drawn and before ImGui::Render.
    void Draw() const;

private:
    float S(float px) const { return px * m_scale; }

    float m_scale;
};

}  // namespace gazeora::app
