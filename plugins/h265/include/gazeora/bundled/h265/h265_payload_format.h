// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gazeora/host/stream/rtp_video.h"

namespace gazeora {

// H.265 NAL unit types (bits 1-6 of the two-byte header).
enum H265NalType : uint8_t {
    kHevcTrailR = 1,
    kHevcIrapFirst = 16,  // 16-21: BLA, IDR and CRA pictures, where decoding can start
    kHevcIdrWRadl = 19,
    kHevcIdrNLp = 20,
    kHevcCra = 21,
    kHevcIrapLast = 21,
    kHevcVps = 32,
    kHevcSps = 33,
    kHevcPps = 34,
    kHevcAp = 48,
    kHevcFu = 49,
};

// H.265 over RTP (RFC 7798), without DONL fields: the SDP sets no sprop-max-don-diff.
class H265PayloadFormat final : public RtpPayloadFormat {
public:
    // Stateless: one instance for the program.
    static const RtpPayloadFormat& Shared();

    VideoCodec Codec() const override { return VideoCodec::H265; }
    const char* EncodingName() const override { return "H265"; }
    uint8_t NalType(const std::string& nal) const override;
    bool IsKeyFrame(const std::string& nal) const override;
    ParameterSet ParameterSetOf(const std::string& nal) const override;
    std::vector<ParameterSet> RequiredParameterSets() const override;
    std::string Fmtp(const std::string sets[4]) const override;
    bool Depacketize(const uint8_t* payload, size_t size, bool& inFragment,
                     std::string& annexB) const override;

protected:
    size_t NalHeaderSize() const override { return 2; }
    // FU: the two-byte NAL header with type 49, keeping the layer and temporal id.
    std::string FragmentPayloadHeader(const std::string& nal) const override;
};

}  // namespace gazeora
