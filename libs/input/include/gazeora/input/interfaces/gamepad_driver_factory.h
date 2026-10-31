// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/input/interfaces/gamepad_driver.h"

namespace gazeora::input {

// Factory pattern: a driver made on the thread that will read it. Throws InputError if game
// controllers can't be read on this PC.
class IGamepadDriverFactory {
public:
    virtual ~IGamepadDriverFactory() = default;

    virtual std::unique_ptr<IGamepadDriver> Create() = 0;
};

}  // namespace gazeora::input
