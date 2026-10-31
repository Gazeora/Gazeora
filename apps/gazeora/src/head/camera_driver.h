// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::app {

// Strategy pattern: how a head turn reaches a game's camera.
class CameraDriver {
public:
    virtual ~CameraDriver() = default;
    // Turns the camera by this much: positive yaw to the left, positive pitch up, in degrees.
    virtual void Turn(double yawDegrees, double pitchDegrees) = 0;
    // The head stopped steering it: forget any part of a turn not passed on yet.
    virtual void Release() = 0;
};

}  // namespace gazeora::app
