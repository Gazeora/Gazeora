// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/host/stream/video_codec_format.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_kit/adapters/plugin_rtp_payload_format.h"

namespace gazeora {

// Adapter pattern: a GazeoraCodecV2 behind VideoCodecFormat: a plugin's codec in Gazeora, or
// the codec of the frames Gazeora hands a bundled plugin's output.
class PluginCodecFormat final : public VideoCodecFormat {
public:
    // `codec` must have passed PluginRegistrar's checks. `keepAlive` (the plugin's DLL, say)
    // is held while the format exists.
    PluginCodecFormat(std::shared_ptr<const void> keepAlive, const GazeoraCodecV2& codec);

    VideoCodec Codec() const override { return m_rtp.Codec(); }
    const RtpPayloadFormat& Rtp() const override { return m_rtp; }
    uint8_t TsStreamType() const override { return m_plugin.ts_stream_type; }
    const std::string& AccessUnitDelimiter() const override { return m_delimiter; }
    bool IsAccessUnitDelimiter(const std::string& nal) const override;
    std::string SeiHeader() const override { return m_seiHeader; }
    bool IsPictureNal(const std::string& nal) const override;
    // GZF1 marks only H.265, by its id; other codecs can't be told apart there.
    uint8_t FrameFlags() const override;

private:
    static std::string Copy(const GazeoraBytesV2& bytes);

    const std::shared_ptr<const void> m_keepAlive;
    const GazeoraCodecV2 m_plugin;
    const PluginRtpPayloadFormat m_rtp;
    const std::string m_delimiter;
    const std::string m_seiHeader;
};

}  // namespace gazeora
