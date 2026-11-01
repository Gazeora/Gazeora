// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/exports/input_forwarder.h"

#include "gazeora/host/stream/camera_status_message.h"
#include "gazeora/host/stream/frame_packet.h"
#include "gazeora/host/stream/head_pose_message.h"
#include "gazeora/host/stream/pointer_message.h"
#include "gazeora/plugin_kit/utils/plugin_struct.h"

namespace gazeora {

void InputForwarder::OnPointer(const PointerEvent& event) {
    Forward(GAZEORA_INPUT_POINTER, PointerMessage::Write(event));
}

void InputForwarder::OnHeadPose(const HeadPose& pose) {
    Forward(GAZEORA_INPUT_HEAD_POSE, HeadPoseMessage::Write(pose));
}

void InputForwarder::OnCameraStatus(const CameraReport& report) {
    Forward(GAZEORA_INPUT_CAMERA_STATUS, CameraStatusMessage::Write(report));
}

void InputForwarder::Forward(uint32_t type, const std::string& message) const {
    if (!PluginStruct::Covers<GazeoraOutputHostV2>(m_host.struct_size) || !m_host.input ||
        message.size() < kFrameHeaderSize) {
        return;
    }
    const auto* payload = reinterpret_cast<const uint8_t*>(message.data()) + kFrameHeaderSize;
    m_host.input(m_host.context, type, payload, message.size() - kFrameHeaderSize);
}

}  // namespace gazeora
