// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gamepad/button_repeat.h"

#include <stdexcept>

namespace gazeora::app {

ButtonRepeat::ButtonRepeat(double delaySeconds, double rateSeconds)
    : m_delay(delaySeconds), m_rate(rateSeconds) {
    if (!(delaySeconds > 0.0 && rateSeconds > 0.0)) {
        throw std::invalid_argument("repeat delay and rate must be above 0");
    }
}

bool ButtonRepeat::Fire(bool held, double nowSeconds) {
    if (!held) {
        m_held = false;
        return false;
    }
    if (!m_held) {
        m_held = true;
        m_nextAt = nowSeconds + m_delay;
        return true;
    }
    if (nowSeconds < m_nextAt) {
        return false;
    }
    m_nextAt += m_rate;
    return true;
}

}  // namespace gazeora::app
