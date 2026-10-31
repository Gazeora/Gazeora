// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::input {

// A controller's sticks, each from -1 to 1: X positive to the right, Y positive down.
enum class GamepadAxis { LeftX, LeftY, RightX, RightY };

constexpr int kGamepadAxisCount = int(GamepadAxis::RightY) + 1;

}  // namespace gazeora::input
