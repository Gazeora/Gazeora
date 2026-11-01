// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/exports/exported_output.h"

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugin_kit/exports/loss_forwarder.h"
#include "gazeora/plugin_kit/utils/plugin_struct.h"
#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

ExportedOutput::ExportedOutput(const FrameOutputFactory& factory, const GazeoraOutputHostV2& host)
    : m_host(host),
      m_log([this](const char* line) {
          if (m_host.log) {
              m_host.log(m_host.context, line);
          }
      }),
      m_input(host),
      m_output(factory.Create(m_log)) {
    m_output->SetPointerSink(&m_input);
    m_output->SetHeadPoseSink(&m_input);
    m_output->SetCameraStatusSink(&m_input);
}

bool ExportedOutput::Start(uint32_t ipv4, uint16_t port, std::string& error) {
    in_addr address = {};
    address.s_addr = ipv4;
    std::wstring wideError;
    if (!m_output->Start(address, port, wideError)) {
        error = Utf8::FromWide(wideError);
        return false;
    }
    return true;
}

void ExportedOutput::Send(const GazeoraFrameV2& frame, const GazeoraLossReporterV2& loss) {
    const VideoCodecFormat* format =
        PluginStruct::Covers<GazeoraFrameV2>(frame.struct_size) ? FormatFor(frame.codec) : nullptr;
    if (!format || !frame.annexb) {
        return;
    }
    m_annexB.assign(reinterpret_cast<const char*>(frame.annexb), frame.size);
    const VideoPipeline::EncodedFrame encoded{m_noPackets,
                                              m_annexB,
                                              frame.timestamp,
                                              frame.key_frame == GAZEORA_TRUE,
                                              VideoCodec(PluginText::Narrow(frame.codec_id)),
                                              *format,
                                              frame.width,
                                              frame.height,
                                              frame.bitrate,
                                              frame.fps};
    LossForwarder forwarder(loss);
    m_output->Send(encoded, forwarder, GetTickCount64());
}

const VideoCodecFormat* ExportedOutput::FormatFor(const GazeoraCodecV2* codec) {
    if (codec != m_formatSource) {
        m_format = codec ? std::make_unique<PluginCodecFormat>(nullptr, *codec) : nullptr;
        m_formatSource = codec;
    }
    return m_format.get();
}

}  // namespace gazeora
