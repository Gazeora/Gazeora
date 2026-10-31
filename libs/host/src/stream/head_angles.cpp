// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/head_angles.h"

#include <algorithm>
#include <cmath>

namespace gazeora {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegreesPerRadian = 180.0 / kPi;
constexpr double kFullTurn = 360.0;
constexpr double kHalfTurn = 180.0;

}  // namespace

HeadAngles HeadAngles::From(const HeadPose& pose) {
    const double x = pose.x, y = pose.y, z = pose.z, w = pose.w;
    // The yaw comes from the head's right, which stays level when looking up or down, where
    // the forward direction's heading would spin.
    const double rightX = 1.0 - 2.0 * (y * y + z * z);
    const double rightZ = 2.0 * (x * z - w * y);
    const double forwardY = -2.0 * (y * z - w * x);
    const double yaw = std::atan2(-rightZ, rightX) * kDegreesPerRadian;
    const double pitch = std::asin(std::clamp(forwardY, -1.0, 1.0)) * kDegreesPerRadian;
    return HeadAngles(yaw, pitch);
}

double HeadAngles::YawSince(const HeadAngles& earlier) const {
    double turn = std::fmod(m_yaw - earlier.m_yaw + kHalfTurn, kFullTurn);
    if (turn < 0.0) {
        turn += kFullTurn;
    }
    return turn - kHalfTurn;
}

}  // namespace gazeora
