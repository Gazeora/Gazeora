// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/input/stick_turn.h"

#include <algorithm>
#include <cmath>

namespace gazeora::hook::input {

namespace {

// Reads further apart than this: the game paused or looked away, so what's pending is stale.
constexpr double kMaxStepSeconds = 0.25;
// Reads closer together count as this far apart, so a burst of them can't ask for a huge push.
constexpr double kMinStepSeconds = 0.002;
// More than this left to turn is a backlog the stick would spin through: let it go.
constexpr double kMaxPendingDegrees = 45.0;
// Less than this of a full push isn't worth moving the stick past the dead zone for.
constexpr double kMinPush = 1e-4;

}  // namespace

StickPush StickTurn::Next(double yaw, double pitch, double seconds, const StickResponse& response) {
    if (!m_started) {
        m_started = true;
        m_yaw = yaw;
        m_pitch = pitch;
        m_seconds = seconds;
        return {};
    }
    m_pendingYaw += yaw - m_yaw;
    m_pendingPitch += pitch - m_pitch;
    m_yaw = yaw;
    m_pitch = pitch;
    const double step = seconds - m_seconds;
    m_seconds = seconds;
    if (step > kMaxStepSeconds || response.degreesPerSecond <= 0.0) {
        m_pendingYaw = m_pendingPitch = 0.0;
        return {};
    }
    m_pendingYaw = std::clamp(m_pendingYaw, -kMaxPendingDegrees, kMaxPendingDegrees);
    m_pendingPitch = std::clamp(m_pendingPitch, -kMaxPendingDegrees, kMaxPendingDegrees);
    const double readStep = std::max(step, kMinStepSeconds);
    StickPush push;
    push.x = -Push(m_pendingYaw, readStep, response);  // a turn to the left is a push left
    push.y = Push(m_pendingPitch, readStep, response);
    return push;
}

double StickTurn::Push(double& pending, double step, const StickResponse& response) {
    const double full = std::clamp(pending / (response.degreesPerSecond * step), -1.0, 1.0);
    pending -= full * response.degreesPerSecond * step;
    if (std::fabs(full) < kMinPush) {
        return 0.0;
    }
    // Past the dead zone, the game's turn grows from nothing to full speed.
    const double deadZone = std::clamp(response.deadZone, 0.0, 0.95);
    return std::copysign(deadZone + (1.0 - deadZone) * std::fabs(full), full);
}

}  // namespace gazeora::hook::input
