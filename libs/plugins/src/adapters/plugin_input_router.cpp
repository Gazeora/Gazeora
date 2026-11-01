// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "adapters/plugin_input_router.h"

#include "gazeora/host/stream/camera_status_message.h"
#include "gazeora/host/stream/head_pose_message.h"
#include "gazeora/host/stream/pointer_message.h"

namespace gazeora {

void PluginInputRouter::Route(uint32_t type, const uint8_t* payload, size_t size) const {
    if (!payload) {
        return;
    }
    PointerEventSink* pointer = m_pointer;
    HeadPoseSink* head = m_head;
    CameraStatusSink* camera = m_camera;
    PointerEvent event;
    HeadPose pose;
    CameraReport report;
    if (type == GAZEORA_INPUT_POINTER && pointer && PointerMessage::Read(payload, size, event)) {
        pointer->OnPointer(event);
    } else if (type == GAZEORA_INPUT_HEAD_POSE && head &&
               HeadPoseMessage::Read(payload, size, pose)) {
        head->OnHeadPose(pose);
    } else if (type == GAZEORA_INPUT_CAMERA_STATUS && camera &&
               CameraStatusMessage::Read(payload, size, report)) {
        camera->OnCameraStatus(report);
    }
}

}  // namespace gazeora
