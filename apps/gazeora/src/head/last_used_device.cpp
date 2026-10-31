// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/last_used_device.h"

namespace gazeora::app {

bool LastUsedDevice::UsingController() const {
    const ControllerTurnChannel::Reads reads = m_controller.Read();
    return reads.recent && reads.activeQpc > m_keyboardMouse.LastUsedQpc();
}

}  // namespace gazeora::app
