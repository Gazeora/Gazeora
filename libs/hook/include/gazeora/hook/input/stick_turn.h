// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::hook::input {

// How the game turns its camera with the right stick: as fast as `degreesPerSecond` at full
// tilt, nothing within `deadZone` (0..1) of the middle.
struct StickResponse {
    double degreesPerSecond = 0.0;
    double deadZone = 0.0;
};

// A push of the stick, each axis -1..1: x to the right, y up.
struct StickPush {
    double x = 0.0;
    double y = 0.0;
};

// Head turns as the right stick: each time the game reads its controller, the turn not yet made
// becomes a push that turns the camera that far by the next read, if the game turns as
// `StickResponse` says. A stick sets the camera's speed, not its angle, so what a push can't
// make up in one read is left for the next.
class StickTurn {
public:
    // `yaw` (to the left) and `pitch` (up) are the head's total turns so far, in degrees;
    // `seconds` the time of this read.
    StickPush Next(double yaw, double pitch, double seconds, const StickResponse& response);

private:
    // The push for one axis, `pending` what's left of it after.
    static double Push(double& pending, double step, const StickResponse& response);

    bool m_started = false;
    double m_yaw = 0.0;
    double m_pitch = 0.0;
    double m_seconds = 0.0;
    double m_pendingYaw = 0.0;
    double m_pendingPitch = 0.0;
};

}  // namespace gazeora::hook::input
