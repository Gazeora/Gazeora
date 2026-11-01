// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/bundled/h265/h265_payload_format.h"

namespace gazeora {

namespace {

constexpr uint8_t kTypeMask = 0x3F;
constexpr uint8_t kHeaderSize = 2;
constexpr uint8_t kFuHeaderSize = 3;
// The NAL header's forbidden bit and the top bit of the layer id, kept in an FU's header.
constexpr uint8_t kForbiddenAndLayerBits = 0x81;

}  // namespace

const RtpPayloadFormat& H265PayloadFormat::Shared() {
    static const H265PayloadFormat format;
    return format;
}

uint8_t H265PayloadFormat::NalType(const std::string& nal) const {
    return nal.empty() ? 0 : uint8_t(nal[0]) >> 1 & kTypeMask;
}

bool H265PayloadFormat::IsKeyFrame(const std::string& nal) const {
    const uint8_t type = NalType(nal);
    return type >= kHevcIrapFirst && type <= kHevcIrapLast;
}

ParameterSet H265PayloadFormat::ParameterSetOf(const std::string& nal) const {
    const uint8_t type = NalType(nal);
    return type == kHevcVps   ? ParameterSet::Vps
           : type == kHevcSps ? ParameterSet::Sps
           : type == kHevcPps ? ParameterSet::Pps
                              : ParameterSet::None;
}

std::vector<ParameterSet> H265PayloadFormat::RequiredParameterSets() const {
    return {ParameterSet::Vps, ParameterSet::Sps, ParameterSet::Pps};
}

std::string H265PayloadFormat::Fmtp(const std::string sets[4]) const {
    const std::string& vps = sets[int(ParameterSet::Vps)];
    const std::string& sps = sets[int(ParameterSet::Sps)];
    const std::string& pps = sets[int(ParameterSet::Pps)];
    if (vps.empty() || sps.empty() || pps.empty()) {
        return {};
    }
    return "sprop-vps=" + Base64(vps) + ";sprop-sps=" + Base64(sps) + ";sprop-pps=" + Base64(pps);
}

bool H265PayloadFormat::Depacketize(const uint8_t* payload, size_t size, bool& inFragment,
                                    std::string& annexB) const {
    if (size < kHeaderSize) return false;
    const uint8_t type = payload[0] >> 1 & kTypeMask;
    if (type < kHevcAp) {
        inFragment = false;
        AppendNal(payload, size, annexB);
        return true;
    }
    if (type == kHevcAp) {
        inFragment = false;
        return UnpackAggregate(payload, size, kHeaderSize, annexB);
    }
    if (type == kHevcFu && size >= kFuHeaderSize) {
        const char header[kHeaderSize] = {
            char((payload[0] & kForbiddenAndLayerBits) | (payload[2] & kTypeMask) << 1),
            char(payload[1])};
        return AddFragment(payload, size, kFuHeaderSize, payload[2], header, kHeaderSize,
                           inFragment, annexB);
    }
    return false;
}

std::string H265PayloadFormat::FragmentPayloadHeader(const std::string& nal) const {
    return {char((nal[0] & kForbiddenAndLayerBits) | kHevcFu << 1), nal[1]};
}

}  // namespace gazeora
