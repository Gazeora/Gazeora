// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::app {

// The right stick scrolls the window being drawn: gently near the center, quickly pushed all
// the way. ImGui's own stick scrolling moves only the window holding the focus, which is
// often a card that can't scroll.
class GamepadScroll {
public:
    explicit GamepadScroll(float scale) : m_scale(scale) {}

    // Call inside the window to scroll, once a frame.
    void Apply() const;

private:
    float m_scale;
};

}  // namespace gazeora::app
