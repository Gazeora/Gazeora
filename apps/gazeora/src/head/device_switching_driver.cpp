// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/device_switching_driver.h"

namespace gazeora::app {

void DeviceSwitchingDriver::Turn(double yawDegrees, double pitchDegrees) {
    const bool onController = m_device.UsingController();
    if (onController != m_onController) {
        Current().Release();
        m_onController = onController;
    }
    Current().Turn(yawDegrees, pitchDegrees);
}

void DeviceSwitchingDriver::Release() {
    m_mouse.Release();
    m_controller.Release();
}

}  // namespace gazeora::app
