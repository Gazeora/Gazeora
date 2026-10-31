// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gamepad/gamepad_presses.h"

#include <gtest/gtest.h>

namespace gazeora::app {
namespace {

using input::GamepadButton;
using input::GamepadState;

GamepadState Holding(GamepadButton button) {
    GamepadState state;
    state.SetButton(button, true);
    return state;
}

TEST(GamepadPressesTest, SeesAPressOnlyInItsFrame) {
    GamepadPresses presses;
    presses.Update({Holding(GamepadButton::Start)});
    EXPECT_TRUE(presses.Pressed(GamepadButton::Start));
    presses.Update({});
    EXPECT_FALSE(presses.Pressed(GamepadButton::Start));
    EXPECT_TRUE(presses.Current().IsDown(GamepadButton::Start));
}

TEST(GamepadPressesTest, SeesATapWithinOneFrame) {
    GamepadPresses presses;
    presses.Update({Holding(GamepadButton::East), GamepadState()});
    EXPECT_TRUE(presses.Pressed(GamepadButton::East));
    EXPECT_FALSE(presses.Holding());
}

TEST(GamepadPressesTest, AHeldButtonIsntPressedAgain) {
    GamepadPresses presses;
    presses.Update({Holding(GamepadButton::South)});
    GamepadState more = Holding(GamepadButton::South);
    more.SetButton(GamepadButton::North, true);
    presses.Update({more});
    EXPECT_FALSE(presses.Pressed(GamepadButton::South));
    EXPECT_TRUE(presses.Pressed(GamepadButton::North));
}

TEST(GamepadPressesTest, ClearLetsGoOfEverything) {
    GamepadPresses presses;
    presses.Update({Holding(GamepadButton::LeftShoulder)});
    presses.Clear();
    EXPECT_FALSE(presses.Pressed(GamepadButton::LeftShoulder));
    EXPECT_FALSE(presses.Holding());
}

}  // namespace
}  // namespace gazeora::app
