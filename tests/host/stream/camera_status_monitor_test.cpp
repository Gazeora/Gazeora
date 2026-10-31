// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/camera_status_monitor.h"

#include <gtest/gtest.h>

namespace gazeora {
namespace {

class CameraStatusMonitorTest : public ::testing::Test {
protected:
    uint64_t nowMs = 5000;
    CameraStatusMonitor monitor{[this] { return nowMs; }};
};

TEST_F(CameraStatusMonitorTest, NothingHeardAtFirst) {
    const CameraReading reading = monitor.Reading();
    EXPECT_FALSE(reading.heard);
    EXPECT_FALSE(reading.fresh);
}

TEST_F(CameraStatusMonitorTest, KeepsTheLatestReportFreshForAFewSeconds) {
    CameraReport report;
    report.state = CameraState::On;
    report.brightness = 90;
    monitor.OnCameraStatus(report);
    nowMs += CameraStatusMonitor::kFreshMs - 1;
    CameraReading reading = monitor.Reading();
    EXPECT_TRUE(reading.fresh);
    EXPECT_EQ(reading.report.state, CameraState::On);
    EXPECT_EQ(reading.report.brightness, 90);

    nowMs += 1;
    reading = monitor.Reading();
    EXPECT_TRUE(reading.heard);
    EXPECT_FALSE(reading.fresh);
    EXPECT_EQ(reading.report.state, CameraState::On) << "kept, for what it said last";
}

}  // namespace
}  // namespace gazeora
