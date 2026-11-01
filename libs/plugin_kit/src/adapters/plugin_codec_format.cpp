// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/adapters/plugin_codec_format.h"

#include "gazeora/host/stream/frame_packet.h"

namespace gazeora {

PluginCodecFormat::PluginCodecFormat(std::shared_ptr<const void> keepAlive,
                                     const GazeoraCodecV2& codec)
    : m_keepAlive(std::move(keepAlive)),
      m_plugin(codec),
      m_rtp(codec),
      m_delimiter(Copy(codec.access_unit_delimiter)),
      m_seiHeader(Copy(codec.sei_header)) {}

std::string PluginCodecFormat::Copy(const GazeoraBytesV2& bytes) {
    return bytes.data ? std::string(reinterpret_cast<const char*>(bytes.data), bytes.size)
                      : std::string();
}

bool PluginCodecFormat::IsAccessUnitDelimiter(const std::string& nal) const {
    return m_plugin.is_access_unit_delimiter(m_plugin.self,
                                             reinterpret_cast<const uint8_t*>(nal.data()),
                                             nal.size()) == GAZEORA_TRUE;
}

uint8_t PluginCodecFormat::FrameFlags() const {
    return Codec() == VideoCodec::H265 ? kFrameFlagH265 : 0;
}

bool PluginCodecFormat::IsPictureNal(const std::string& nal) const {
    return m_plugin.is_picture(m_plugin.self, reinterpret_cast<const uint8_t*>(nal.data()),
                               nal.size()) == GAZEORA_TRUE;
}

}  // namespace gazeora
