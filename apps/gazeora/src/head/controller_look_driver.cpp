// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/controller_look_driver.h"

#include <algorithm>

namespace gazeora::app {

void ControllerLookDriver::SetStick(float degreesPerSecond, float deadZone) {
    m_degreesPerSecond = std::clamp(degreesPerSecond, kMinDegreesPerSecond, kMaxDegreesPerSecond);
    m_deadZone = std::clamp(deadZone, 0.0f, kMaxDeadZone);
    m_channel.SetStick(m_degreesPerSecond, m_deadZone);
}

void ControllerLookDriver::Turn(double yawDegrees, double pitchDegrees) {
    m_channel.AddTurn(yawDegrees, pitchDegrees);
}

}  // namespace gazeora::app
