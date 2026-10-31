// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/device_switching_driver.h"

#include <gtest/gtest.h>

#include "app/head/head_fakes.h"
#include "head/player_device.h"

namespace gazeora::app {
namespace {

class FixedDevice final : public PlayerDevice {
public:
    bool UsingController() const override { return controller; }

    bool controller = false;
};

class DeviceSwitchingDriverTest : public ::testing::Test {
protected:
    RecordingCamera mouse;
    RecordingCamera controller;
    FixedDevice device;
    DeviceSwitchingDriver driver{mouse, controller, device};
};

TEST_F(DeviceSwitchingDriverTest, TurnsThroughTheMouseOnKeyboardAndMouse) {
    driver.Turn(2.0, 1.0);
    EXPECT_EQ(mouse.turns, 1);
    EXPECT_EQ(controller.turns, 0);
    EXPECT_DOUBLE_EQ(mouse.yaw, 2.0);
}

TEST_F(DeviceSwitchingDriverTest, TurnsThroughTheControllerOnIt) {
    device.controller = true;
    driver.Turn(-3.0, 0.5);
    EXPECT_EQ(mouse.turns, 0);
    EXPECT_EQ(controller.turns, 1);
    EXPECT_DOUBLE_EQ(controller.pitch, 0.5);
}

TEST_F(DeviceSwitchingDriverTest, ReleasesTheOneItLeaves) {
    driver.Turn(1.0, 0.0);
    device.controller = true;
    driver.Turn(1.0, 0.0);
    EXPECT_EQ(mouse.releases, 1);
    EXPECT_EQ(controller.releases, 0);
    device.controller = false;
    driver.Turn(1.0, 0.0);
    EXPECT_EQ(controller.releases, 1);
    EXPECT_EQ(mouse.turns, 2);
}

TEST_F(DeviceSwitchingDriverTest, ReleaseReleasesBoth) {
    driver.Release();
    EXPECT_EQ(mouse.releases, 1);
    EXPECT_EQ(controller.releases, 1);
}

}  // namespace
}  // namespace gazeora::app
