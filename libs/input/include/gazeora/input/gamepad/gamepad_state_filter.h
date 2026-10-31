// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/input/models/gamepad_state.h"

namespace gazeora::input {

// Steadies the sticks: a stick at rest reads a little off center on most controllers, and
// its last digits flicker. Inside the dead zone it reads 0; past it, the rest of the travel
// is spread from 0 to 1 again, in coarse steps, so only real moves change the state.
class GamepadStateFilter {
public:
    static constexpr float kDefaultDeadZone = 0.2f;
    static constexpr float kDefaultStep = 1.0f / 32.0f;

    GamepadStateFilter() = default;
    // Throws std::invalid_argument unless 0 <= deadZone < 1 and 0 < step <= 1.
    GamepadStateFilter(float deadZone, float step);

    GamepadState Apply(const GamepadState& raw) const;

private:
    float Steady(float value) const;

    float m_deadZone = kDefaultDeadZone;
    float m_step = kDefaultStep;
};

}  // namespace gazeora::input
