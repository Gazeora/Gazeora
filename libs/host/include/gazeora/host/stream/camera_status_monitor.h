// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>

#include "gazeora/host/stream/camera_report.h"
#include "gazeora/host/stream/camera_status_sink.h"

namespace gazeora {

// What the phone says of its camera lately, for the overlay.
struct CameraReading {
    bool heard = false;  // a report ever came
    bool fresh = false;  // one came in the last few seconds: the phone repeats it every second
    CameraReport report;
};

// Keeps the phone's latest camera status report.
class CameraStatusMonitor final : public CameraStatusSink {
public:
    using Clock = std::function<uint64_t()>;  // milliseconds
    static constexpr uint64_t kFreshMs = 3000;

    // `clock` defaults to GetTickCount64.
    explicit CameraStatusMonitor(Clock clock = {});

    void OnCameraStatus(const CameraReport& report) override;
    // From any thread.
    CameraReading Reading() const;

private:
    Clock m_clock;
    mutable std::mutex m_mutex;
    CameraReport m_last;
    uint64_t m_lastMs = 0;
    bool m_heard = false;
};

}  // namespace gazeora
