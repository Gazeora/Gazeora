// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>

#include "gazeora/host/stream/head_pose.h"
#include "gazeora/host/stream/head_pose_sink.h"

namespace gazeora {

// What the phone's head tracking sends lately, for the overlay.
struct HeadReading {
    bool fresh = false;  // a pose came in the last second
    float yaw = 0.0f;    // degrees, as HeadAngles has them
    float pitch = 0.0f;
    int perSecond = 0;  // poses in the last whole second
};

// Decorator pattern: notes each head pose on its way to `next`.
class HeadPoseMonitor final : public HeadPoseSink {
public:
    using Clock = std::function<uint64_t()>;  // milliseconds

    // `clock` defaults to GetTickCount64.
    explicit HeadPoseMonitor(HeadPoseSink& next, Clock clock = {});

    void OnHeadPose(const HeadPose& pose) override;
    // From any thread.
    HeadReading Reading() const;

private:
    void Note(const HeadPose& pose);

    HeadPoseSink& m_next;
    Clock m_clock;
    mutable std::mutex m_mutex;
    HeadPose m_last;
    uint64_t m_lastMs = 0;
    uint64_t m_secondStartMs = 0;
    int m_inSecond = 0;
    int m_perSecond = 0;
};

}  // namespace gazeora
