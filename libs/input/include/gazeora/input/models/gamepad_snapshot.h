// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

#include "gazeora/input/models/gamepad_info.h"

namespace gazeora::input {

// Unavailable: nothing can read controllers, as no plugin offers a driver.
enum class GamepadSessionState { Off, Running, Error, Unavailable };

// What reading game controllers is doing right now, for the UI.
struct GamepadSnapshot {
    GamepadSessionState state = GamepadSessionState::Off;
    std::vector<GamepadInfo> connected;
    std::wstring message;  // why, in the Error state
};

}  // namespace gazeora::input
