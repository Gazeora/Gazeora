// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/host/stream/camera_status_monitor.h"

namespace gazeora {

// The overlay's line on the phone's camera, e.g. "Camera: on  ·  light 42%".
class CameraOverlayLine {
public:
    // Empty until the phone first reports, so players other than the app don't get it.
    static std::wstring From(const CameraReading& reading);
};

}  // namespace gazeora
