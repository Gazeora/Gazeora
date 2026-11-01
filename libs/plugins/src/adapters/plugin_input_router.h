// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "gazeora/host/stream/camera_status_sink.h"
#include "gazeora/host/stream/head_pose_sink.h"
#include "gazeora/host/stream/pointer_event_sink.h"
#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// The input a plugin output hands over (GazeoraOutputHostV2::input), read as the app's
// messages and passed to the sinks the stream server set. Messages that don't read are
// dropped. Thread-safe.
class PluginInputRouter final {
public:
    void SetPointerSink(PointerEventSink* sink) { m_pointer = sink; }
    void SetHeadPoseSink(HeadPoseSink* sink) { m_head = sink; }
    void SetCameraStatusSink(CameraStatusSink* sink) { m_camera = sink; }

    // One message: `type` is GAZEORA_INPUT_*, `payload` that GZF1 message's.
    void Route(uint32_t type, const uint8_t* payload, size_t size) const;

private:
    std::atomic<PointerEventSink*> m_pointer{nullptr};
    std::atomic<HeadPoseSink*> m_head{nullptr};
    std::atomic<CameraStatusSink*> m_camera{nullptr};
};

}  // namespace gazeora
