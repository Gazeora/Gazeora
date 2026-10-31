// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gamepad/gamepad_presses.h"

namespace gazeora::app {

using input::GamepadButton;
using input::GamepadState;

void GamepadPresses::Update(const std::vector<GamepadState>& states) {
    m_pressed.reset();
    for (const GamepadState& state : states) {
        for (int i = 0; i < input::kGamepadButtonCount; ++i) {
            GamepadButton button = GamepadButton(i);
            if (state.IsDown(button) && !m_current.IsDown(button)) {
                m_pressed.set(size_t(i));
            }
        }
        m_current = state;
    }
}

void GamepadPresses::Clear() {
    m_pressed.reset();
    m_current = GamepadState();
}

bool GamepadPresses::Pressed(GamepadButton button) const { return m_pressed.test(size_t(button)); }

}  // namespace gazeora::app
