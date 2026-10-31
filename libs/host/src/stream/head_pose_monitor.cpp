// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/head_pose_monitor.h"

#include <windows.h>

#include "gazeora/host/stream/head_angles.h"

namespace gazeora {

namespace {

constexpr uint64_t kSecondMs = 1000;

}  // namespace

HeadPoseMonitor::HeadPoseMonitor(HeadPoseSink& next, Clock clock)
    : m_next(next), m_clock(clock ? std::move(clock) : Clock([] { return GetTickCount64(); })) {}

void HeadPoseMonitor::OnHeadPose(const HeadPose& pose) {
    Note(pose);
    m_next.OnHeadPose(pose);
}

void HeadPoseMonitor::Note(const HeadPose& pose) {
    const uint64_t now = m_clock();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (now - m_secondStartMs >= kSecondMs) {
        // A gap of more than a second leaves nothing to count from the one before.
        m_perSecond = now - m_secondStartMs < 2 * kSecondMs ? m_inSecond : 0;
        m_secondStartMs = now;
        m_inSecond = 0;
    }
    ++m_inSecond;
    m_last = pose;
    m_lastMs = now;
}

HeadReading HeadPoseMonitor::Reading() const {
    const uint64_t now = m_clock();
    std::lock_guard<std::mutex> lock(m_mutex);
    HeadReading reading;
    reading.fresh = m_lastMs != 0 && now - m_lastMs < kSecondMs;
    if (!reading.fresh) {
        return reading;
    }
    const HeadAngles angles = HeadAngles::From(m_last);
    reading.yaw = float(angles.Yaw());
    reading.pitch = float(angles.Pitch());
    reading.perSecond = m_perSecond;
    return reading;
}

}  // namespace gazeora
