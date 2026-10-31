// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/head_pose_gate.h"

namespace gazeora {

void HeadPoseGate::OnHeadPose(const HeadPose& pose) {
    HeadPoseSink* target = m_target;
    if (target) {
        target->OnHeadPose(pose);
    }
}

}  // namespace gazeora
