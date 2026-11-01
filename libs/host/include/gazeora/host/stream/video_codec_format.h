// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <string>

#include "gazeora/host/encode/video_codec.h"
#include "gazeora/host/stream/rtp_video.h"

namespace gazeora {

// Strategy pattern: everything the stream does that depends on the codec, apart from
// encoding: RTP and SDP, MPEG-TS, the view info SEI, and the GZF1 frame flags. One per codec,
// stateless and shared, registered with its VideoCodecInfo. The stream server and its outputs
// only ever ask the format, never which codec they have.
class VideoCodecFormat {
public:
    virtual ~VideoCodecFormat() = default;

    virtual VideoCodec Codec() const = 0;
    // Its RTP payload format, which also tells its NAL unit types and parameter sets.
    virtual const RtpPayloadFormat& Rtp() const = 0;

    // Its MPEG-TS stream_type (ISO/IEC 13818-1, table 2-34).
    virtual uint8_t TsStreamType() const = 0;
    // An access unit delimiter NAL unit, with its start code: a transport stream needs one in
    // front of every access unit.
    virtual const std::string& AccessUnitDelimiter() const = 0;
    virtual bool IsAccessUnitDelimiter(const std::string& nal) const = 0;

    // The NAL unit header of an SEI message, which the view info goes in (stream_view_info.h);
    // empty if the codec can't carry it, and frames go without.
    virtual std::string SeiHeader() const = 0;
    // Whether `nal` carries picture data (a VCL NAL unit), which SEI has to come before.
    virtual bool IsPictureNal(const std::string& nal) const = 0;

    // Its flags in the GZF1 frame header (frame_packet.h).
    virtual uint8_t FrameFlags() const = 0;
};

}  // namespace gazeora
