// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>

namespace gazeora {

// Where the viewer's head points, from the phone in the VR viewer: a unit quaternion turning
// the head's axes into the world's. The world has Y up and a heading of its own (only turns
// from it count); the head looks down its -Z, with X to its right and Y up, as OpenGL's camera.
struct HeadPose {
    // Counts up with every pose the phone sends, so an older one arriving late is told apart.
    uint32_t sample = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

}  // namespace gazeora
