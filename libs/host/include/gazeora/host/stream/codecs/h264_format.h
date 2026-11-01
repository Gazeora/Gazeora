// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/host/stream/video_codec_format.h"

namespace gazeora {

// H.264 (ITU-T H.264): RTP per RFC 6184, packetization mode 1.
class H264Format final : public VideoCodecFormat {
public:
    // The one instance, for the program's life.
    static std::shared_ptr<const VideoCodecFormat> Shared();

    VideoCodec Codec() const override { return VideoCodec::H264; }
    const RtpPayloadFormat& Rtp() const override { return RtpPayloadFormat::H264(); }
    uint8_t TsStreamType() const override;
    const std::string& AccessUnitDelimiter() const override { return m_delimiter; }
    bool IsAccessUnitDelimiter(const std::string& nal) const override;
    std::string SeiHeader() const override;
    bool IsPictureNal(const std::string& nal) const override;
    uint8_t FrameFlags() const override { return 0; }

private:
    const std::string m_delimiter{"\0\0\0\1\x09\xF0", 6};
};

}  // namespace gazeora
