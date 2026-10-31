// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "head/camera_driver.h"
#include "head/player_device.h"

namespace gazeora::app {

// Strategy pattern: each turn through the mouse or the controller, whichever the player is on,
// so the game never sees the other and switches its prompts and controls to it.
class DeviceSwitchingDriver final : public CameraDriver {
public:
    DeviceSwitchingDriver(CameraDriver& mouse, CameraDriver& controller, const PlayerDevice& device)
        : m_mouse(mouse), m_controller(controller), m_device(device) {}

    void Turn(double yawDegrees, double pitchDegrees) override;
    void Release() override;

private:
    CameraDriver& Current() { return m_onController ? m_controller : m_mouse; }

    CameraDriver& m_mouse;
    CameraDriver& m_controller;
    const PlayerDevice& m_device;
    bool m_onController = false;
};

}  // namespace gazeora::app
