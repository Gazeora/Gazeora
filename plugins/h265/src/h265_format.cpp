// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/bundled/h265/h265_format.h"

#include "gazeora/host/stream/frame_packet.h"

namespace gazeora {

namespace {

constexpr uint8_t kStreamType = 0x24;
constexpr uint8_t kNalAccessUnitDelimiter = 35;
constexpr uint8_t kNalPrefixSei = 39;
// The second header byte: layer 0, temporal id 0 (plus 1).
constexpr char kLayerAndTemporalId = '\x01';
// VCL NAL unit types, the ones with picture data, are 0..31.
constexpr uint8_t kFirstNonPictureType = 32;
// H.265 gets about the same picture as H.264 from this share of its bits.
constexpr uint32_t kBitratePercent = 70;

}  // namespace

std::shared_ptr<const VideoCodecFormat> H265Format::Shared() {
    static const std::shared_ptr<const VideoCodecFormat> instance = std::make_shared<H265Format>();
    return instance;
}

VideoCodecInfo H265Format::Info() {
    return {VideoCodec::H265, "H.265", "H.265 - sharper on weak Wi-Fi, newer devices only",
            kBitratePercent, Shared()};
}

uint8_t H265Format::TsStreamType() const { return kStreamType; }

bool H265Format::IsAccessUnitDelimiter(const std::string& nal) const {
    return !nal.empty() && Rtp().NalType(nal) == kNalAccessUnitDelimiter;
}

std::string H265Format::SeiHeader() const {
    return std::string{char(kNalPrefixSei << 1), kLayerAndTemporalId};
}

bool H265Format::IsPictureNal(const std::string& nal) const {
    return !nal.empty() && Rtp().NalType(nal) < kFirstNonPictureType;
}

uint8_t H265Format::FrameFlags() const { return kFrameFlagH265; }

}  // namespace gazeora
