// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/input/gamepad/gamepad_state_filter.h"

#include <gtest/gtest.h>

#include <stdexcept>

namespace gazeora::input {
namespace {

GamepadState LeftStickAt(float x) {
    GamepadState state;
    state.SetAxis(GamepadAxis::LeftX, x);
    return state;
}

TEST(GamepadStateFilterTest, ReadsADriftingStickAsCentered) {
    GamepadStateFilter filter;
    // A DualSense at rest, as SDL reads it: -3470 of 32767.
    EXPECT_TRUE(filter.Apply(LeftStickAt(-0.106f)).IsNeutral());
}

TEST(GamepadStateFilterTest, SpreadsTheRestOfTheTravel) {
    GamepadStateFilter filter;
    EXPECT_EQ(filter.Apply(LeftStickAt(1.0f)).Axis(GamepadAxis::LeftX), 1.0f);
    EXPECT_EQ(filter.Apply(LeftStickAt(-1.0f)).Axis(GamepadAxis::LeftX), -1.0f);
    EXPECT_NEAR(filter.Apply(LeftStickAt(0.6f)).Axis(GamepadAxis::LeftX), 0.5f,
                GamepadStateFilter::kDefaultStep);
}

TEST(GamepadStateFilterTest, IgnoresFlickerInTheLastDigits) {
    GamepadStateFilter filter;
    EXPECT_EQ(filter.Apply(LeftStickAt(0.7f)), filter.Apply(LeftStickAt(0.7004f)));
}

TEST(GamepadStateFilterTest, LeavesButtonsAlone) {
    GamepadStateFilter filter;
    GamepadState pressed;
    pressed.SetButton(GamepadButton::South, true);
    EXPECT_TRUE(filter.Apply(pressed).IsDown(GamepadButton::South));
}

TEST(GamepadStateFilterTest, RefusesADeadZoneOrStepOutOfRange) {
    EXPECT_THROW(GamepadStateFilter(1.0f, 0.1f), std::invalid_argument);
    EXPECT_THROW(GamepadStateFilter(-0.1f, 0.1f), std::invalid_argument);
    EXPECT_THROW(GamepadStateFilter(0.2f, 0.0f), std::invalid_argument);
}

}  // namespace
}  // namespace gazeora::input
