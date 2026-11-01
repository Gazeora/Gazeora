// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "services/plugin_codec_check.h"

namespace gazeora {

namespace {

constexpr uint32_t kSmallestNalHeader = 1;
constexpr uint32_t kLargestNalHeader = 2;

}  // namespace

std::wstring PluginCodecCheck::Problem(const GazeoraCodecV2* codec) {
    if (!codec || codec->struct_size < sizeof(GazeoraCodecV2) || !HasFunctions(*codec) ||
        !HasText(*codec)) {
        return L"a codec with missing fields";
    }
    if (codec->nal_header_size < kSmallestNalHeader || codec->nal_header_size > kLargestNalHeader) {
        return L"a codec whose NAL unit header isn't 1 or 2 bytes";
    }
    if (!codec->access_unit_delimiter.data || codec->access_unit_delimiter.size == 0) {
        return L"a codec without an access unit delimiter";
    }
    if (codec->bitrate_percent == 0) {
        return L"a codec that needs no bitrate";
    }
    return {};
}

bool PluginCodecCheck::HasFunctions(const GazeoraCodecV2& codec) {
    return codec.nal_type && codec.is_key_frame && codec.is_picture &&
           codec.is_access_unit_delimiter && codec.parameter_set_of && codec.fmtp &&
           codec.fragment_payload_header;
}

bool PluginCodecCheck::HasText(const GazeoraCodecV2& codec) {
    return codec.name && codec.choice && codec.rtp_encoding_name;
}

}  // namespace gazeora
