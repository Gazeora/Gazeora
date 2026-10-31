// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/input/platform/sdl_gamepad_driver.h"

#include <gtest/gtest.h>

namespace gazeora::input {
namespace {

// Whatever is plugged in: SDL starts and reads without a window, and names what it finds.
TEST(SdlGamepadDriverTest, ReadsWithoutAWindow) {
    SdlGamepadDriver driver;
    driver.Update();
    for (const GamepadInfo& gamepad : driver.Connected()) {
        EXPECT_FALSE(gamepad.name.empty());
    }
}

// Every session makes a new driver: SDL has to start again after it was shut down.
TEST(SdlGamepadDriverTest, StartsAgainAfterShuttingDown) {
    {
        SdlGamepadDriver first;
    }
    SdlGamepadDriver second;
    EXPECT_NO_THROW(second.Update());
}

}  // namespace
}  // namespace gazeora::input
