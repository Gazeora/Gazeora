// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "head/camera_driver.h"
#include "head/game_focus.h"
#include "head/mouse_mover.h"
#include "support/head_poses.h"

namespace gazeora::app {

class RecordingMouse final : public MouseMover {
public:
    void Move(int dx, int dy) override {
        x += dx;
        y += dy;
    }

    int x = 0;
    int y = 0;
};

class RecordingCamera final : public CameraDriver {
public:
    void Turn(double yawDegrees, double pitchDegrees) override {
        yaw += yawDegrees;
        pitch += pitchDegrees;
        ++turns;
    }
    void Release() override { ++releases; }

    double yaw = 0.0;
    double pitch = 0.0;
    int turns = 0;
    int releases = 0;
};

class FixedFocus final : public GameFocus {
public:
    bool GameInFront() const override { return inFront; }

    bool inFront = true;
};

}  // namespace gazeora::app
