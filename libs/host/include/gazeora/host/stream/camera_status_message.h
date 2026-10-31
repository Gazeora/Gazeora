// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "gazeora/host/stream/camera_report.h"

namespace gazeora {

// The payload of a GZF1 CameraStatus message (frame_packet.h), after the header whose last
// word is the payload length, to kPointerPort over the network:
//    0  u8  state (CameraState)
//    1  u8  brightness, the picture's mean luma
// The Android app has its own copy (protocols/camera_status_message.h); keep the two in step.
class CameraStatusMessage {
public:
    static constexpr size_t kPayloadSize = 2;

    // False if the payload is short or its state unknown.
    static bool Read(const uint8_t* payload, size_t size, CameraReport& out);
    // A whole message, header and payload, as the app sends it (for tests).
    static std::string Write(const CameraReport& report);
};

}  // namespace gazeora
