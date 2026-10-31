// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/host/capture/controller_turn_channel.h"
#include "head/camera_driver.h"

namespace gazeora::app {

// Turns the camera with the game's controller: each turn goes to the hook in the game, which
// moves the right stick as the game reads it, so the game stays on its controller prompts and
// controls. A stick sets how fast the camera turns, so how the game turns at full tilt, and what
// it ignores near the middle, are the user's to set.
class ControllerLookDriver final : public CameraDriver {
public:
    static constexpr float kMinDegreesPerSecond = 30.0f;
    static constexpr float kMaxDegreesPerSecond = 720.0f;
    static constexpr float kDefaultDegreesPerSecond = 180.0f;
    static constexpr float kMaxDeadZone = 0.5f;
    static constexpr float kDefaultDeadZone = 0.15f;

    explicit ControllerLookDriver(ControllerTurnChannel& channel) : m_channel(channel) {}

    // Clamped to the ranges above.
    void SetStick(float degreesPerSecond, float deadZone);
    float DegreesPerSecond() const { return m_degreesPerSecond; }
    float DeadZone() const { return m_deadZone; }

    void Turn(double yawDegrees, double pitchDegrees) override;
    // The hook drops what's left to turn on its own when the head stops steering.
    void Release() override {}

private:
    ControllerTurnChannel& m_channel;
    float m_degreesPerSecond = kDefaultDegreesPerSecond;
    float m_deadZone = kDefaultDeadZone;
};

}  // namespace gazeora::app
