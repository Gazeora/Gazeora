#include "gazeora/input/models/gamepad_state.h"

#include <gtest/gtest.h>

namespace gazeora::input {
namespace {

TEST(GamepadStateTest, StartsNeutral) {
    GamepadState state;
    EXPECT_TRUE(state.IsNeutral());
    EXPECT_FALSE(state.IsDown(GamepadButton::South));
    EXPECT_EQ(state.Axis(GamepadAxis::LeftX), 0.0f);
}

TEST(GamepadStateTest, KeepsButtonsAndClampsSticks) {
    GamepadState state;
    state.SetButton(GamepadButton::DpadUp, true);
    state.SetAxis(GamepadAxis::RightY, -1.5f);
    EXPECT_TRUE(state.IsDown(GamepadButton::DpadUp));
    EXPECT_FALSE(state.IsDown(GamepadButton::DpadDown));
    EXPECT_EQ(state.Axis(GamepadAxis::RightY), -1.0f);
    EXPECT_FALSE(state.IsNeutral());
}

TEST(GamepadStateTest, MergesButtonsAndTheFurthestStick) {
    GamepadState first;
    first.SetButton(GamepadButton::South, true);
    first.SetAxis(GamepadAxis::LeftX, 0.3f);
    GamepadState second;
    second.SetButton(GamepadButton::East, true);
    second.SetAxis(GamepadAxis::LeftX, -0.8f);
    second.SetAxis(GamepadAxis::LeftY, 0.1f);

    first.Merge(second);
    EXPECT_TRUE(first.IsDown(GamepadButton::South));
    EXPECT_TRUE(first.IsDown(GamepadButton::East));
    EXPECT_EQ(first.Axis(GamepadAxis::LeftX), -0.8f);
    EXPECT_EQ(first.Axis(GamepadAxis::LeftY), 0.1f);
}

TEST(GamepadStateTest, ComparesEveryButtonAndStick) {
    GamepadState a;
    GamepadState b;
    EXPECT_EQ(a, b);
    b.SetAxis(GamepadAxis::LeftY, 0.5f);
    EXPECT_NE(a, b);
    a.SetAxis(GamepadAxis::LeftY, 0.5f);
    a.SetButton(GamepadButton::Start, true);
    EXPECT_NE(a, b);
}

}  // namespace
}  // namespace gazeora::input
