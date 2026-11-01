// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/plugin_api/gazeora_plugin.h"
#include "recording.h"

namespace frame_recorder {

// Factory Method: the protocol Gazeora lists in its settings, making a RecorderOutput each
// time it's chosen.
class RecorderProtocol {
public:
    explicit RecorderProtocol(Recording& recording);
    // The struct points back at this object, so it stays where it is.
    RecorderProtocol(const RecorderProtocol&) = delete;
    RecorderProtocol& operator=(const RecorderProtocol&) = delete;

    const GazeoraProtocolV2& Struct() const;

private:
    static int32_t GAZEORA_CALL CreateOutput(void* self, const GazeoraOutputHostV2* host,
                                             GazeoraOutputV2* output);

    Recording& m_recording;
    GazeoraProtocolV2 m_protocol;
};

}  // namespace frame_recorder
