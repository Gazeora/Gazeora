// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>

namespace gazeora {

// How the phone's back camera (for hand tracking) stands. The numbers are on the wire.
enum class CameraState : uint8_t {
    Off = 0,          // the hands aren't in use
    Starting = 1,     // opening
    On = 2,           // looking for hands
    Dark = 3,         // off to save power: too dark to see hands
    Checking = 4,     // on for a moment, to see whether the light came back
    Unavailable = 5,  // couldn't open: no permission, in use elsewhere, or none
};
constexpr uint8_t kCameraStateCount = 6;

struct CameraReport {
    CameraState state = CameraState::Off;
    uint8_t brightness = 0;  // the picture's mean luma, 0..255, as last seen
};

}  // namespace gazeora
