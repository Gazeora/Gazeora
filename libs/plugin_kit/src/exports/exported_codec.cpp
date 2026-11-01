// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/exports/exported_codec.h"

#include <algorithm>
#include <cstring>

#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

namespace {

uint32_t ToPlugin(ParameterSet set) {
    switch (set) {
        case ParameterSet::Vps:
            return GAZEORA_PARAMETER_SET_VPS;
        case ParameterSet::Sps:
            return GAZEORA_PARAMETER_SET_SPS;
        case ParameterSet::Pps:
            return GAZEORA_PARAMETER_SET_PPS;
        default:
            return GAZEORA_PARAMETER_SET_NONE;
    }
}

GazeoraBytesV2 BytesOf(const std::string& text) {
    return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

}  // namespace

ExportedCodec::ExportedCodec(VideoCodecInfo info)
    : m_info(std::move(info)), m_id(m_info.id.Id()), m_seiHeader(m_info.format->SeiHeader()) {
    FillDescription();
    FillCalls();
}

void ExportedCodec::FillDescription() {
    const RtpPayloadFormat& rtp = m_info.format->Rtp();
    m_codec.struct_size = sizeof(m_codec);
    m_codec.self = this;
    m_codec.id = m_id.c_str();
    m_codec.name = m_info.name.c_str();
    m_codec.choice = m_info.choice.c_str();
    m_codec.bitrate_percent = m_info.bitratePercent;
    m_codec.rtp_encoding_name = rtp.EncodingName();
    m_codec.nal_header_size = uint32_t(rtp.NalHeaderSize());
    const std::vector<ParameterSet> required = rtp.RequiredParameterSets();
    for (size_t i = 0; i < required.size() && i < GAZEORA_PARAMETER_SET_COUNT - 1; ++i) {
        m_codec.required_parameter_sets[i] = ToPlugin(required[i]);
    }
    m_codec.ts_stream_type = m_info.format->TsStreamType();
    m_codec.access_unit_delimiter = BytesOf(m_info.format->AccessUnitDelimiter());
    m_codec.sei_header = BytesOf(m_seiHeader);
}

void ExportedCodec::FillCalls() {
    m_codec.nal_type = NalType;
    m_codec.is_key_frame = IsKeyFrame;
    m_codec.is_picture = IsPicture;
    m_codec.is_access_unit_delimiter = IsDelimiter;
    m_codec.parameter_set_of = ParameterSetOf;
    m_codec.fmtp = Fmtp;
    m_codec.fragment_payload_header = FragmentHeader;
    m_codec.depacketize = Depacketize;
}

const VideoCodecFormat& ExportedCodec::FormatOf(void* self) {
    return *static_cast<const ExportedCodec*>(self)->m_info.format;
}

std::string ExportedCodec::Text(const uint8_t* data, size_t size) {
    return data ? std::string(reinterpret_cast<const char*>(data), size) : std::string();
}

uint32_t GAZEORA_CALL ExportedCodec::NalType(void* self, const uint8_t* nal, size_t size) {
    return FormatOf(self).Rtp().NalType(Text(nal, size));
}

int32_t GAZEORA_CALL ExportedCodec::IsKeyFrame(void* self, const uint8_t* nal, size_t size) {
    return FormatOf(self).Rtp().IsKeyFrame(Text(nal, size)) ? GAZEORA_TRUE : GAZEORA_FALSE;
}

int32_t GAZEORA_CALL ExportedCodec::IsPicture(void* self, const uint8_t* nal, size_t size) {
    return FormatOf(self).IsPictureNal(Text(nal, size)) ? GAZEORA_TRUE : GAZEORA_FALSE;
}

int32_t GAZEORA_CALL ExportedCodec::IsDelimiter(void* self, const uint8_t* nal, size_t size) {
    return FormatOf(self).IsAccessUnitDelimiter(Text(nal, size)) ? GAZEORA_TRUE : GAZEORA_FALSE;
}

uint32_t GAZEORA_CALL ExportedCodec::ParameterSetOf(void* self, const uint8_t* nal, size_t size) {
    return ToPlugin(FormatOf(self).Rtp().ParameterSetOf(Text(nal, size)));
}

void GAZEORA_CALL ExportedCodec::Fmtp(void* self, const GazeoraBytesV2* sets, char* fmtp,
                                      size_t capacity) {
    std::string copies[GAZEORA_PARAMETER_SET_COUNT];
    for (uint32_t i = 0; i < GAZEORA_PARAMETER_SET_COUNT; ++i) {
        copies[i] = Text(sets[i].data, sets[i].size);
    }
    PluginText::Write(FormatOf(self).Rtp().Fmtp(copies), fmtp, capacity);
}

size_t GAZEORA_CALL ExportedCodec::FragmentHeader(void* self, const uint8_t* nal, size_t size,
                                                  uint8_t* header, size_t capacity) {
    const std::string payloadHeader = FormatOf(self).Rtp().FragmentPayloadHeader(Text(nal, size));
    const size_t length = std::min(payloadHeader.size(), capacity);
    std::memcpy(header, payloadHeader.data(), length);
    return length;
}

int32_t GAZEORA_CALL ExportedCodec::Depacketize(void* self, const uint8_t* payload, size_t size,
                                                int32_t* inFragment, GazeoraBytesSinkV2 sink,
                                                void* sinkContext) {
    bool fragment = *inFragment != GAZEORA_FALSE;
    std::string annexB;
    const bool ok = FormatOf(self).Rtp().Depacketize(payload, size, fragment, annexB);
    *inFragment = fragment ? GAZEORA_TRUE : GAZEORA_FALSE;
    if (!annexB.empty()) {
        sink(sinkContext, reinterpret_cast<const uint8_t*>(annexB.data()), annexB.size());
    }
    return ok ? GAZEORA_TRUE : GAZEORA_FALSE;
}

}  // namespace gazeora
