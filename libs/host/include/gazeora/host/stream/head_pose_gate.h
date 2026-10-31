// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <atomic>

#include "gazeora/host/stream/head_pose_sink.h"

namespace gazeora {

// Decorator pattern: passes head poses on to a target, once there is one.
class HeadPoseGate final : public HeadPoseSink {
public:
    // Null drops every pose. Set before the stream starts.
    void SetTarget(HeadPoseSink* target) { m_target = target; }

    void OnHeadPose(const HeadPose& pose) override;

private:
    std::atomic<HeadPoseSink*> m_target{nullptr};
};

}  // namespace gazeora
