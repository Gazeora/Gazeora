// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/input/gamepad/gamepad_state_filter.h"

#include <cmath>
#include <stdexcept>

namespace gazeora::input {

GamepadStateFilter::GamepadStateFilter(float deadZone, float step)
    : m_deadZone(deadZone), m_step(step) {
    if (!(deadZone >= 0.0f && deadZone < 1.0f)) {
        throw std::invalid_argument("dead zone must be from 0 up to 1");
    }
    if (!(step > 0.0f && step <= 1.0f)) {
        throw std::invalid_argument("step must be above 0 and at most 1");
    }
}

GamepadState GamepadStateFilter::Apply(const GamepadState& raw) const {
    GamepadState steady = raw;
    for (int i = 0; i < kGamepadAxisCount; ++i) {
        GamepadAxis axis = GamepadAxis(i);
        steady.SetAxis(axis, Steady(raw.Axis(axis)));
    }
    return steady;
}

float GamepadStateFilter::Steady(float value) const {
    float magnitude = std::fabs(value);
    if (magnitude <= m_deadZone) {
        return 0.0f;
    }
    float spread = (magnitude - m_deadZone) / (1.0f - m_deadZone);
    float stepped = std::round(spread / m_step) * m_step;
    return std::copysign(stepped, value);
}

}  // namespace gazeora::input
