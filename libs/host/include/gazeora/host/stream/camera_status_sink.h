// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/host/stream/camera_report.h"

namespace gazeora {

// Where the phone's camera status reports go. Called from the stream's threads.
class CameraStatusSink {
public:
    virtual ~CameraStatusSink() = default;
    virtual void OnCameraStatus(const CameraReport& report) = 0;
};

}  // namespace gazeora
