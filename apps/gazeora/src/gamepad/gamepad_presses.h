// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <bitset>
#include <vector>

#include "gazeora/input/models/gamepad_state.h"

namespace gazeora::app {

// The buttons pressed since the last frame, and what the controllers do now, for what ImGui's
// navigation doesn't cover: pages, going back, menus and the on-screen keyboard.
class GamepadPresses {
public:
    // Once a frame, with the states since the last one (none: nothing changed).
    void Update(const std::vector<input::GamepadState>& states);
    // Lets go of everything, while the controllers mustn't drive the window.
    void Clear();

    // Went down this frame, even if it's already up again.
    bool Pressed(input::GamepadButton button) const;
    const input::GamepadState& Current() const { return m_current; }
    bool Holding() const { return !m_current.IsNeutral(); }

private:
    input::GamepadState m_current;
    std::bitset<input::kGamepadButtonCount> m_pressed;
};

}  // namespace gazeora::app
