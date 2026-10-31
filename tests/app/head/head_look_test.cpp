// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/head_look.h"

#include <gtest/gtest.h>

#include "app/head/head_fakes.h"

namespace gazeora::app {
namespace {

constexpr double kTolerance = 1e-2;

class HeadLookTest : public ::testing::Test {
protected:
    HeadLookTest() { look.SetEnabled(true); }

    RecordingCamera camera;
    FixedFocus focus;
    uint64_t nowMs = 10000;
    HeadLook look{camera, focus, [this] { return nowMs; }};
};

TEST_F(HeadLookTest, TurnsTheCameraByTheHeadsTurns) {
    look.OnHeadPose(PoseAt(1, 30.0, 5.0));  // where the head starts: no turn
    EXPECT_EQ(camera.turns, 0);
    look.OnHeadPose(PoseAt(2, 35.0, 3.0));
    look.OnHeadPose(PoseAt(3, 38.0, 0.0));
    EXPECT_NEAR(camera.yaw, 8.0, kTolerance);
    EXPECT_NEAR(camera.pitch, -5.0, kTolerance);
}

TEST_F(HeadLookTest, SkipsPosesThatArriveLate) {
    look.OnHeadPose(PoseAt(10, 0.0, 0.0));
    look.OnHeadPose(PoseAt(12, 4.0, 0.0));
    look.OnHeadPose(PoseAt(11, 2.0, 0.0));
    look.OnHeadPose(PoseAt(13, 6.0, 0.0));
    EXPECT_NEAR(camera.yaw, 6.0, kTolerance);
    EXPECT_EQ(camera.turns, 2);
}

TEST_F(HeadLookTest, StartsOverWithoutATurnWhenThePhoneRestarts) {
    look.OnHeadPose(PoseAt(5000, 0.0, 0.0));
    look.OnHeadPose(PoseAt(5001, 2.0, 0.0));
    look.OnHeadPose(PoseAt(1, 20.0, 0.0));  // a new heading
    look.OnHeadPose(PoseAt(2, 21.0, 0.0));
    EXPECT_NEAR(camera.yaw, 3.0, kTolerance);
}

TEST_F(HeadLookTest, FollowsTheSampleCountAcrossItsWrap) {
    look.OnHeadPose(PoseAt(0xFFFFFFFFu, 0.0, 0.0));
    look.OnHeadPose(PoseAt(0, 1.0, 0.0));
    EXPECT_NEAR(camera.yaw, 1.0, kTolerance);
}

TEST_F(HeadLookTest, StartsOverWithoutATurnAfterAPause) {
    look.OnHeadPose(PoseAt(1, 0.0, 0.0));
    nowMs += 251;
    look.OnHeadPose(PoseAt(2, 10.0, 0.0));  // the sensor may have a new heading
    nowMs += 5;
    look.OnHeadPose(PoseAt(3, 12.0, 0.0));
    EXPECT_NEAR(camera.yaw, 2.0, kTolerance);
}

TEST_F(HeadLookTest, IgnoresJumpsNoHeadMakes) {
    look.OnHeadPose(PoseAt(1, 0.0, 0.0));
    look.OnHeadPose(PoseAt(2, 90.0, 0.0));
    look.OnHeadPose(PoseAt(3, 91.0, 0.0));
    EXPECT_NEAR(camera.yaw, 1.0, kTolerance);
}

TEST_F(HeadLookTest, LeavesTheCameraAloneWhileTheGameIsntInFront) {
    look.OnHeadPose(PoseAt(1, 0.0, 0.0));
    focus.inFront = false;
    look.OnHeadPose(PoseAt(2, 10.0, 0.0));
    EXPECT_EQ(camera.turns, 0);
    EXPECT_EQ(camera.releases, 1);
    focus.inFront = true;
    look.OnHeadPose(PoseAt(3, 15.0, 0.0));  // starts again from here
    look.OnHeadPose(PoseAt(4, 16.0, 0.0));
    EXPECT_NEAR(camera.yaw, 1.0, kTolerance);
}

TEST_F(HeadLookTest, LeavesTheCameraAloneWhileSwitchedOff) {
    look.SetEnabled(false);
    look.OnHeadPose(PoseAt(1, 0.0, 0.0));
    look.OnHeadPose(PoseAt(2, 10.0, 0.0));
    EXPECT_EQ(camera.turns, 0);
}

TEST_F(HeadLookTest, ReceivingForASecondAfterThePhonesLastPose) {
    EXPECT_FALSE(look.Receiving());
    look.OnHeadPose(PoseAt(1, 0.0, 0.0));
    nowMs += 999;
    EXPECT_TRUE(look.Receiving());
    nowMs += 1;
    EXPECT_FALSE(look.Receiving());
}

}  // namespace
}  // namespace gazeora::app
