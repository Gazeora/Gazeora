// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <vector>

#include "gazeora/input/models/gamepad_info.h"
#include "gazeora/input/models/gamepad_state.h"

namespace gazeora::input {

// The game controllers connected to this PC. Used from one thread only: the one that made it.
class IGamepadDriver {
public:
    virtual ~IGamepadDriver() = default;

    // Takes in controllers plugged in or out, and their latest readings.
    virtual void Update() = 0;
    virtual std::vector<GamepadInfo> Connected() const = 0;
    // What every connected controller does, merged: any of them can drive.
    virtual GamepadState State() const = 0;
};

}  // namespace gazeora::input
