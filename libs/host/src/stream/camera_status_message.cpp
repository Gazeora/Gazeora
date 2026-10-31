// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/camera_status_message.h"

#include "gazeora/host/stream/frame_packet.h"

namespace gazeora {

bool CameraStatusMessage::Read(const uint8_t* payload, size_t size, CameraReport& out) {
    if (size < kPayloadSize || payload[0] >= kCameraStateCount) {
        return false;
    }
    out.state = CameraState(payload[0]);
    out.brightness = payload[1];
    return true;
}

std::string CameraStatusMessage::Write(const CameraReport& report) {
    const char payload[kPayloadSize] = {char(report.state), char(report.brightness)};
    FrameHeader header;
    header.type = FrameMessage::CameraStatus;
    return StreamMessage(header, std::string(payload, sizeof(payload)));
}

}  // namespace gazeora
