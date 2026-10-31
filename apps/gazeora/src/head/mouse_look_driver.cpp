// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/mouse_look_driver.h"

#include <algorithm>
#include <cmath>

namespace gazeora::app {

void MouseLookDriver::SetCountsPerDegree(float counts) {
    m_countsPerDegree = std::clamp(counts, kMinCountsPerDegree, kMaxCountsPerDegree);
}

void MouseLookDriver::Turn(double yawDegrees, double pitchDegrees) {
    const double counts = m_countsPerDegree;
    // A turn to the left is a move to the left; looking up is a move up, which is -y.
    int dx = TakeCounts(-yawDegrees * counts, m_carriedX);
    int dy = TakeCounts(-pitchDegrees * counts, m_carriedY);
    m_mouse.Move(dx, dy);
}

void MouseLookDriver::Release() {
    m_carriedX = 0.0;
    m_carriedY = 0.0;
}

int MouseLookDriver::TakeCounts(double wanted, double& carried) {
    const double total = wanted + carried;
    const double whole = std::trunc(total);
    carried = total - whole;
    return int(whole);
}

}  // namespace gazeora::app
