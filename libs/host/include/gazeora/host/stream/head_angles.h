// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/host/stream/head_pose.h"

namespace gazeora {

// Which way the head points, in degrees: yaw to the left of the world's heading, pitch above
// the horizon. Roll is left out: a mouse can't tilt a camera.
class HeadAngles {
public:
    static HeadAngles From(const HeadPose& pose);

    double Yaw() const { return m_yaw; }
    double Pitch() const { return m_pitch; }
    // The turn from `earlier` to these, yaw the short way round (-180..180).
    double YawSince(const HeadAngles& earlier) const;
    double PitchSince(const HeadAngles& earlier) const { return m_pitch - earlier.m_pitch; }

private:
    HeadAngles(double yaw, double pitch) : m_yaw(yaw), m_pitch(pitch) {}

    double m_yaw;
    double m_pitch;
};

}  // namespace gazeora
