// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/codecs/h264_format.h"

namespace gazeora {

namespace {

constexpr uint8_t kStreamType = 0x1B;
constexpr uint8_t kNalAccessUnitDelimiter = 9;
constexpr uint8_t kNalSei = 6;
// Picture data: non-IDR slices (1) through IDR slices (5).
constexpr uint8_t kFirstPictureType = 1;
constexpr uint8_t kLastPictureType = 5;

}  // namespace

std::shared_ptr<const VideoCodecFormat> H264Format::Shared() {
    static const std::shared_ptr<const VideoCodecFormat> instance = std::make_shared<H264Format>();
    return instance;
}

uint8_t H264Format::TsStreamType() const { return kStreamType; }

bool H264Format::IsAccessUnitDelimiter(const std::string& nal) const {
    return !nal.empty() && Rtp().NalType(nal) == kNalAccessUnitDelimiter;
}

std::string H264Format::SeiHeader() const { return std::string(1, char(kNalSei)); }

bool H264Format::IsPictureNal(const std::string& nal) const {
    if (nal.empty()) {
        return false;
    }
    const uint8_t type = Rtp().NalType(nal);
    return type >= kFirstPictureType && type <= kLastPictureType;
}

}  // namespace gazeora
