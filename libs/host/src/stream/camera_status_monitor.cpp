// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/camera_status_monitor.h"

#include <windows.h>

namespace gazeora {

CameraStatusMonitor::CameraStatusMonitor(Clock clock)
    : m_clock(clock ? std::move(clock) : Clock([] { return GetTickCount64(); })) {}

void CameraStatusMonitor::OnCameraStatus(const CameraReport& report) {
    const uint64_t now = m_clock();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_last = report;
    m_lastMs = now;
    m_heard = true;
}

CameraReading CameraStatusMonitor::Reading() const {
    const uint64_t now = m_clock();
    std::lock_guard<std::mutex> lock(m_mutex);
    CameraReading reading;
    reading.heard = m_heard;
    reading.fresh = m_heard && now - m_lastMs < kFreshMs;
    reading.report = m_last;
    return reading;
}

}  // namespace gazeora
