// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <array>
#include <bitset>

#include "gazeora/input/models/gamepad_axis.h"
#include "gazeora/input/models/gamepad_button.h"

namespace gazeora::input {

// What a controller's buttons and sticks do at one moment. Neutral when made.
class GamepadState {
public:
    bool IsDown(GamepadButton button) const { return m_buttons.test(size_t(button)); }
    float Axis(GamepadAxis axis) const { return m_axes[size_t(axis)]; }
    bool IsNeutral() const;

    void SetButton(GamepadButton button, bool down) { m_buttons.set(size_t(button), down); }
    // Clamped to -1..1.
    void SetAxis(GamepadAxis axis, float value);
    // Adds another controller's: its buttons held too, and each stick pushed the furthest.
    void Merge(const GamepadState& other);

    bool operator==(const GamepadState& other) const;
    bool operator!=(const GamepadState& other) const { return !(*this == other); }

private:
    std::bitset<kGamepadButtonCount> m_buttons;
    std::array<float, kGamepadAxisCount> m_axes{};
};

}  // namespace gazeora::input
