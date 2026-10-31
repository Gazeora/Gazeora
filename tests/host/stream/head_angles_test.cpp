// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/head_angles.h"

#include <gtest/gtest.h>

#include "support/head_poses.h"

namespace gazeora {
namespace {

constexpr double kTolerance = 1e-3;

TEST(HeadAnglesTest, LookingAheadIsZero) {
    HeadAngles angles = HeadAngles::From(HeadPose{});
    EXPECT_NEAR(angles.Yaw(), 0.0, kTolerance);
    EXPECT_NEAR(angles.Pitch(), 0.0, kTolerance);
}

TEST(HeadAnglesTest, ReadsYawAndPitch) {
    HeadAngles angles = HeadAngles::From(PoseAt(0, 40.0, -25.0));
    EXPECT_NEAR(angles.Yaw(), 40.0, kTolerance);
    EXPECT_NEAR(angles.Pitch(), -25.0, kTolerance);
}

TEST(HeadAnglesTest, KeepsTheYawWhenLookingNearlyStraightUp) {
    HeadAngles angles = HeadAngles::From(PoseAt(0, -70.0, 89.0));
    EXPECT_NEAR(angles.Yaw(), -70.0, kTolerance);
    EXPECT_NEAR(angles.Pitch(), 89.0, 0.05);
}

TEST(HeadAnglesTest, TurnsTheShortWayAcrossBehind) {
    HeadAngles before = HeadAngles::From(PoseAt(0, 175.0, 0.0));
    HeadAngles after = HeadAngles::From(PoseAt(1, -175.0, 0.0));
    EXPECT_NEAR(after.YawSince(before), 10.0, kTolerance);
    EXPECT_NEAR(before.YawSince(after), -10.0, kTolerance);
}

TEST(HeadAnglesTest, MeasuresPitchChanges) {
    HeadAngles before = HeadAngles::From(PoseAt(0, 0.0, 10.0));
    HeadAngles after = HeadAngles::From(PoseAt(1, 0.0, 25.0));
    EXPECT_NEAR(after.PitchSince(before), 15.0, kTolerance);
}

}  // namespace
}  // namespace gazeora
