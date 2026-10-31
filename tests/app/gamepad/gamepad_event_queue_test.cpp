// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gamepad/gamepad_event_queue.h"

#include <gtest/gtest.h>

namespace gazeora::app {
namespace {

using input::GamepadAxis;
using input::GamepadState;

GamepadState LeftStickAt(float x) {
    GamepadState state;
    state.SetAxis(GamepadAxis::LeftX, x);
    return state;
}

TEST(GamepadEventQueueTest, WakesTheUiForEachChange) {
    int wakes = 0;
    GamepadEventQueue queue([&] { ++wakes; });
    queue.OnGamepadState(LeftStickAt(0.5f));
    queue.OnGamepadState(GamepadState());
    EXPECT_EQ(wakes, 2);
}

TEST(GamepadEventQueueTest, HandsOverTheStatesOnceInOrder) {
    GamepadEventQueue queue([] {});
    queue.OnGamepadState(LeftStickAt(0.5f));
    queue.OnGamepadState(LeftStickAt(1.0f));
    std::vector<GamepadState> taken = queue.Take();
    ASSERT_EQ(taken.size(), 2u);
    EXPECT_EQ(taken[0], LeftStickAt(0.5f));
    EXPECT_EQ(taken[1], LeftStickAt(1.0f));
    EXPECT_TRUE(queue.Take().empty());
}

TEST(GamepadEventQueueTest, KeepsTheLatestWhenTheUiFallsBehind) {
    GamepadEventQueue queue([] {});
    for (int i = 0; i < 1000; ++i) {
        queue.OnGamepadState(LeftStickAt(0.5f));
    }
    queue.OnGamepadState(GamepadState());
    std::vector<GamepadState> taken = queue.Take();
    EXPECT_LT(taken.size(), 1000u);
    EXPECT_TRUE(taken.back().IsNeutral());
}

}  // namespace
}  // namespace gazeora::app
