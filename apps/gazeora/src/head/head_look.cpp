// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/head_look.h"

#include <windows.h>

#include <cmath>

namespace gazeora::app {

namespace {

constexpr uint64_t kReceivingWithinMs = 1000;
// Datagrams can arrive out of order; a pose this many samples behind is a late one. Farther
// behind, the phone started counting again.
constexpr int32_t kMaxLateSamples = 64;
// After a pause this long the phone's sensor may have a new heading: start from the next pose.
constexpr uint64_t kMaxPauseMs = 250;
// More than this between two poses is the sensor resetting, not a head: start from the new one.
constexpr double kMaxStepDegrees = 30.0;

}  // namespace

HeadLook::HeadLook(CameraDriver& camera, const GameFocus& focus, Clock clock)
    : m_camera(camera),
      m_focus(focus),
      m_clock(clock ? std::move(clock) : Clock([] { return GetTickCount64(); })) {}

bool HeadLook::Receiving() const {
    const uint64_t last = m_lastPoseMs;
    return last != 0 && m_clock() - last < kReceivingWithinMs;
}

void HeadLook::OnHeadPose(const HeadPose& pose) {
    const uint64_t now = m_clock();
    m_lastPoseMs = now;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (Steering()) {
        Steer(pose, now);
    } else {
        Stop();
    }
}

int32_t HeadLook::SamplesAhead(const HeadPose& pose) const {
    return int32_t(pose.sample - m_last->sample);
}

void HeadLook::Steer(const HeadPose& pose, uint64_t nowMs) {
    const HeadAngles angles = HeadAngles::From(pose);
    if (m_last && nowMs - m_last->atMs > kMaxPauseMs) {
        Stop();
    }
    if (m_last) {
        const int32_t ahead = SamplesAhead(pose);
        if (ahead <= 0 && ahead >= -kMaxLateSamples) {
            return;
        }
        // After a restart the phone's heading is a new one: start from it, without a turn.
        if (ahead > 0) {
            TurnFrom(m_last->angles, angles);
        }
    }
    m_last = Last{pose.sample, angles, nowMs};
}

void HeadLook::TurnFrom(const HeadAngles& earlier, const HeadAngles& now) {
    const double yaw = now.YawSince(earlier);
    const double pitch = now.PitchSince(earlier);
    if (std::fabs(yaw) <= kMaxStepDegrees && std::fabs(pitch) <= kMaxStepDegrees) {
        m_camera.Turn(yaw, pitch);
    }
}

void HeadLook::Stop() {
    if (m_last) {
        m_last.reset();
        m_camera.Release();
    }
}

}  // namespace gazeora::app
