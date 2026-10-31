// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gamepad/button_repeat.h"

#include <gtest/gtest.h>

#include <stdexcept>

namespace gazeora::app {
namespace {

constexpr double kDelay = 0.4;
constexpr double kRate = 0.1;

TEST(ButtonRepeatTest, FiresOnPressThenRepeatsAfterThePause) {
    ButtonRepeat repeat(kDelay, kRate);
    EXPECT_TRUE(repeat.Fire(true, 0.0));
    EXPECT_FALSE(repeat.Fire(true, 0.2));
    EXPECT_TRUE(repeat.Fire(true, 0.41));
    EXPECT_FALSE(repeat.Fire(true, 0.45));
    EXPECT_TRUE(repeat.Fire(true, 0.51));
}

TEST(ButtonRepeatTest, StartsOverOnceLetGo) {
    ButtonRepeat repeat(kDelay, kRate);
    EXPECT_TRUE(repeat.Fire(true, 0.0));
    EXPECT_FALSE(repeat.Fire(false, 0.1));
    EXPECT_TRUE(repeat.Fire(true, 0.2));
}

TEST(ButtonRepeatTest, RefusesTimesThatArentPositive) {
    EXPECT_THROW(ButtonRepeat(0.0, kRate), std::invalid_argument);
    EXPECT_THROW(ButtonRepeat(kDelay, -1.0), std::invalid_argument);
}

}  // namespace
}  // namespace gazeora::app
