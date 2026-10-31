// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/host/stream/head_pose.h"

namespace gazeora {

// Where head poses from the phone go. Called from the stream's threads.
class HeadPoseSink {
public:
    virtual ~HeadPoseSink() = default;
    virtual void OnHeadPose(const HeadPose& pose) = 0;
};

}  // namespace gazeora
