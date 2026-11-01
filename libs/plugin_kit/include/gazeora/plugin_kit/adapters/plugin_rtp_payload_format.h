// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

#include "gazeora/host/stream/rtp_video.h"
#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// Adapter pattern: a plugin codec's NAL unit rules behind RtpPayloadFormat. The packets are
// built by RtpPayloadFormat::Packetize, as for the built-in codecs.
class PluginRtpPayloadFormat final : public RtpPayloadFormat {
public:
    // `codec` must have passed PluginRegistrar's checks.
    explicit PluginRtpPayloadFormat(const GazeoraCodecV2& codec);

    VideoCodec Codec() const override { return m_codec; }
    const char* EncodingName() const override { return m_encodingName.c_str(); }
    uint8_t NalType(const std::string& nal) const override;
    bool IsKeyFrame(const std::string& nal) const override;
    ParameterSet ParameterSetOf(const std::string& nal) const override;
    std::vector<ParameterSet> RequiredParameterSets() const override { return m_required; }
    std::string Fmtp(const std::string sets[4]) const override;
    bool Depacketize(const uint8_t* payload, size_t size, bool& inFragment,
                     std::string& annexB) const override;
    size_t NalHeaderSize() const override { return m_plugin.nal_header_size; }
    std::string FragmentPayloadHeader(const std::string& nal) const override;

private:
    static const uint8_t* Bytes(const std::string& text) {
        return reinterpret_cast<const uint8_t*>(text.data());
    }
    static void GAZEORA_CALL Append(void* annexB, const uint8_t* data, size_t size);
    static std::vector<ParameterSet> RequiredOf(const GazeoraCodecV2& codec);

    const GazeoraCodecV2 m_plugin;
    const VideoCodec m_codec;
    const std::string m_encodingName;
    const std::vector<ParameterSet> m_required;
};

}  // namespace gazeora
