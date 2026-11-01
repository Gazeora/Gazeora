// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/host/stream/frame_output_factory.h"
#include "gazeora/host/stream/stream_protocol_info.h"
#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// Adapter pattern: one of Gazeora's own protocols, its StreamProtocolInfo and the
// FrameOutputFactory that makes its outputs, as a GazeoraProtocolV2 for a bundled plugin to
// register. The struct points back at it, so it can't be copied or moved.
class ExportedProtocol final {
public:
    ExportedProtocol(StreamProtocolInfo info, std::shared_ptr<const FrameOutputFactory> outputs);
    ExportedProtocol(const ExportedProtocol&) = delete;
    ExportedProtocol& operator=(const ExportedProtocol&) = delete;

    const GazeoraProtocolV2& Protocol() const { return m_protocol; }

private:
    static int32_t GAZEORA_CALL CreateOutput(void* self, const GazeoraOutputHostV2* host,
                                             GazeoraOutputV2* output);

    const StreamProtocolInfo m_info;
    const std::shared_ptr<const FrameOutputFactory> m_outputs;
    GazeoraProtocolV2 m_protocol = {};
};

}  // namespace gazeora
