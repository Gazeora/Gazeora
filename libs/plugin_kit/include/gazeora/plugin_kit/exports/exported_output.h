// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/stream/frame_output.h"
#include "gazeora/host/stream/frame_output_factory.h"
#include "gazeora/host/stream/stream_log.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_kit/adapters/plugin_codec_format.h"
#include "gazeora/plugin_kit/exports/input_forwarder.h"

namespace gazeora {

// Adapter pattern: one of Gazeora's own outputs, a FrameOutput, as the state behind a
// GazeoraOutputV2 in a bundled plugin (ExportedOutputCalls fills the struct). Its log lines,
// its players' input and their loss go back to Gazeora through the host; each frame's codec
// comes as a GazeoraCodecV2 and reaches the output as a PluginCodecFormat.
class ExportedOutput final {
public:
    // A new, stopped output from `factory`. `host` outlives it.
    ExportedOutput(const FrameOutputFactory& factory, const GazeoraOutputHostV2& host);
    ExportedOutput(const ExportedOutput&) = delete;
    ExportedOutput& operator=(const ExportedOutput&) = delete;

    FrameOutput& Output() { return *m_output; }
    // FrameOutput::Start, with the address and the error as the plugin interface has them.
    bool Start(uint32_t ipv4, uint16_t port, std::string& error);
    // FrameOutput::Send for one frame Gazeora hands over. A frame without a codec is dropped.
    void Send(const GazeoraFrameV2& frame, const GazeoraLossReporterV2& loss);

private:
    // The format of `codec`, remade when the frames switch codec; null for none.
    const VideoCodecFormat* FormatFor(const GazeoraCodecV2* codec);

    const GazeoraOutputHostV2& m_host;
    StreamLog m_log;
    InputForwarder m_input;
    std::unique_ptr<FrameOutput> m_output;

    // Send's, on the encode thread.
    const GazeoraCodecV2* m_formatSource = nullptr;
    std::unique_ptr<PluginCodecFormat> m_format;
    std::string m_annexB;
    const std::vector<std::string> m_noPackets;  // RTP packets: only RTSP takes them
};

}  // namespace gazeora
