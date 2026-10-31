// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/head_pose_monitor.h"

#include <gtest/gtest.h>

#include "support/head_poses.h"

namespace gazeora {
namespace {

class CountingSink final : public HeadPoseSink {
public:
    void OnHeadPose(const HeadPose& pose) override {
        last = pose;
        ++poses;
    }

    HeadPose last;
    int poses = 0;
};

class HeadPoseMonitorTest : public ::testing::Test {
protected:
    CountingSink next;
    uint64_t nowMs = 5000;
    HeadPoseMonitor monitor{next, [this] { return nowMs; }};
};

TEST_F(HeadPoseMonitorTest, PassesEveryPoseOn) {
    monitor.OnHeadPose(PoseAt(7, 10.0, 0.0));
    EXPECT_EQ(next.poses, 1);
    EXPECT_EQ(next.last.sample, 7u);
}

TEST_F(HeadPoseMonitorTest, ReadsTheLatestAnglesWhileFresh) {
    EXPECT_FALSE(monitor.Reading().fresh);
    monitor.OnHeadPose(PoseAt(1, 10.0, 0.0));
    monitor.OnHeadPose(PoseAt(2, 25.0, -12.0));
    HeadReading reading = monitor.Reading();
    ASSERT_TRUE(reading.fresh);
    EXPECT_NEAR(reading.yaw, 25.0f, 0.05f);
    EXPECT_NEAR(reading.pitch, -12.0f, 0.05f);
    nowMs += 1000;
    EXPECT_FALSE(monitor.Reading().fresh);
}

TEST_F(HeadPoseMonitorTest, CountsPosesASecond) {
    for (int i = 0; i < 200; ++i) {
        monitor.OnHeadPose(PoseAt(uint32_t(i), 0.0, 0.0));
        nowMs += 5;
    }
    monitor.OnHeadPose(PoseAt(200, 0.0, 0.0));  // the next second starts
    EXPECT_EQ(monitor.Reading().perSecond, 200);
}

}  // namespace
}  // namespace gazeora
