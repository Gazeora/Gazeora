// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/host/stream/camera_status_sink.h"
#include "gazeora/host/stream/head_pose_sink.h"
#include "gazeora/host/stream/pointer_event_sink.h"
#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// Adapter pattern: the input sinks an output in a bundled plugin hands its players' touches,
// head poses and camera status to. Each goes back to Gazeora through the host's input, as the
// message the app sent.
class InputForwarder final : public PointerEventSink, public HeadPoseSink, public CameraStatusSink {
public:
    // `host` outlives the forwarder.
    explicit InputForwarder(const GazeoraOutputHostV2& host) : m_host(host) {}

    void OnPointer(const PointerEvent& event) override;
    void OnHeadPose(const HeadPose& pose) override;
    void OnCameraStatus(const CameraReport& report) override;

private:
    // Hands on the payload of `message`, a whole GZF1 message.
    void Forward(uint32_t type, const std::string& message) const;

    const GazeoraOutputHostV2& m_host;
};

}  // namespace gazeora
