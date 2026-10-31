// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cmath>
#include <cstdint>

#include "gazeora/host/stream/head_pose.h"

namespace gazeora {

// The head turned `yaw` degrees left, then `pitch` degrees up.
inline HeadPose PoseAt(uint32_t sample, double yaw, double pitch) {
    constexpr double kPi = 3.14159265358979323846;
    const double halfYaw = yaw * kPi / 360.0;
    const double halfPitch = pitch * kPi / 360.0;
    // Yaw about the world's Y, then pitch about the head's X.
    const double cy = std::cos(halfYaw), sy = std::sin(halfYaw);
    const double cp = std::cos(halfPitch), sp = std::sin(halfPitch);
    HeadPose pose;
    pose.sample = sample;
    pose.w = float(cy * cp);
    pose.x = float(cy * sp);
    pose.y = float(sy * cp);
    pose.z = float(-sy * sp);
    return pose;
}

}  // namespace gazeora
