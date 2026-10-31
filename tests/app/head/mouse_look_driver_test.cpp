// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/mouse_look_driver.h"

#include <gtest/gtest.h>

#include "app/head/head_fakes.h"

namespace gazeora::app {
namespace {

TEST(MouseLookDriverTest, TurningLeftAndUpMovesLeftAndUp) {
    RecordingMouse mouse;
    MouseLookDriver driver(mouse);
    driver.SetCountsPerDegree(10.0f);
    driver.Turn(2.0, 3.0);
    EXPECT_EQ(mouse.x, -20);
    EXPECT_EQ(mouse.y, -30);
}

TEST(MouseLookDriverTest, CarriesPartsOfACountOver) {
    RecordingMouse mouse;
    MouseLookDriver driver(mouse);
    driver.SetCountsPerDegree(1.0f);
    for (int i = 0; i < 10; ++i) {
        driver.Turn(-0.25, 0.0);
    }
    EXPECT_EQ(mouse.x, 2);  // 2.5 counts so far, half a count still carried
    driver.Turn(-0.5, 0.0);
    EXPECT_EQ(mouse.x, 3);
}

TEST(MouseLookDriverTest, ReleaseDropsWhatIsCarried) {
    RecordingMouse mouse;
    MouseLookDriver driver(mouse);
    driver.SetCountsPerDegree(1.0f);
    driver.Turn(-0.75, 0.0);
    driver.Release();
    driver.Turn(-0.75, 0.0);
    EXPECT_EQ(mouse.x, 0);
}

TEST(MouseLookDriverTest, ClampsTheSensitivity) {
    RecordingMouse mouse;
    MouseLookDriver driver(mouse);
    driver.SetCountsPerDegree(0.0f);
    EXPECT_FLOAT_EQ(driver.CountsPerDegree(), MouseLookDriver::kMinCountsPerDegree);
    driver.SetCountsPerDegree(1000.0f);
    EXPECT_FLOAT_EQ(driver.CountsPerDegree(), MouseLookDriver::kMaxCountsPerDegree);
}

}  // namespace
}  // namespace gazeora::app
