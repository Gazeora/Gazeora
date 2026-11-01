// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "recorder_protocol.h"

#include <memory>

#include "guarded.h"
#include "recorder_output.h"

namespace frame_recorder {

namespace {

// Kept the same across versions: stream.ini stores it.
constexpr char kId[] = "recorder";
constexpr char kName[] = "Record to file";
constexpr char kHelp[] =
    "Writes each frame of the game to an .h264 or .h265 file instead of streaming it. VLC plays "
    "the file.";
constexpr char kDiscoveryPath[] = "/";

}  // namespace

RecorderProtocol::RecorderProtocol(Recording& recording) : m_recording(recording), m_protocol() {
    m_protocol.struct_size = sizeof(GazeoraProtocolV2);
    m_protocol.self = this;
    m_protocol.id = kId;
    m_protocol.name = kName;
    m_protocol.help = kHelp;
    m_protocol.discovery_path = kDiscoveryPath;
    m_protocol.opened_by_players = GAZEORA_FALSE;  // no player opens a URL of it
    m_protocol.create_output = CreateOutput;
}

const GazeoraProtocolV2& RecorderProtocol::Struct() const { return m_protocol; }

int32_t GAZEORA_CALL RecorderProtocol::CreateOutput(void* self, const GazeoraOutputHostV2* host,
                                                    GazeoraOutputV2* output) {
    return Guarded::Call<int32_t>(GAZEORA_FALSE, [&] {
        auto& protocol = *static_cast<RecorderProtocol*>(self);
        auto recorder = std::make_unique<RecorderOutput>(protocol.m_recording, *host);
        *output = recorder->Struct();
        static_cast<void>(recorder.release());  // Gazeora owns it now: it calls destroy
        return GAZEORA_TRUE;
    });
}

}  // namespace frame_recorder
