// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::input {

// A controller's buttons by where they sit, so every make reads the same: South is A on an
// Xbox pad, Cross on a PlayStation one and B on a Nintendo one. The guide button is left
// out: Windows and Steam keep it for themselves.
enum class GamepadButton {
    South,
    East,
    West,
    North,
    Back,
    Start,
    LeftStick,
    RightStick,
    LeftShoulder,
    RightShoulder,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
};

constexpr int kGamepadButtonCount = int(GamepadButton::DpadRight) + 1;

}  // namespace gazeora::input
