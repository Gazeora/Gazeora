// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/camera_overlay_line.h"

#include <cwchar>

namespace gazeora {

namespace {

// By CameraState.
constexpr const wchar_t* kStateText[kCameraStateCount] = {
    L"Camera: off",
    L"Camera: starting",
    L"Camera: on",
    L"Camera: off, too dark for hands",
    L"Camera: checking the light",
    L"Camera: unavailable",
};
constexpr wchar_t kNoWord[] = L"Camera: no word from the phone";

}  // namespace

std::wstring CameraOverlayLine::From(const CameraReading& reading) {
    if (!reading.heard) {
        return {};
    }
    const CameraState state = reading.report.state;
    // Off is said once, as the hands stop; the rest are repeated while they last.
    if (!reading.fresh && state != CameraState::Off) {
        return kNoWord;
    }
    std::wstring line = kStateText[uint8_t(state)];
    if (state == CameraState::On) {
        wchar_t light[32];
        swprintf(light, 32, L"  ·  light %d%%", reading.report.brightness * 100 / 255);
        line += light;
    }
    return line;
}

}  // namespace gazeora
