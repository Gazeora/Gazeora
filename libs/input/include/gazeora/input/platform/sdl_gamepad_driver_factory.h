// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/input/interfaces/gamepad_driver_factory.h"
#include "gazeora/input/platform/sdl_gamepad_driver.h"

namespace gazeora::input {

// Factory pattern: an SDL driver for each session.
class SdlGamepadDriverFactory final : public IGamepadDriverFactory {
public:
    std::unique_ptr<IGamepadDriver> Create() override {
        return std::make_unique<SdlGamepadDriver>();
    }
};

}  // namespace gazeora::input
