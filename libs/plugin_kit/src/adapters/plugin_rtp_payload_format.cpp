// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/adapters/plugin_rtp_payload_format.h"

#include <algorithm>
#include <cstring>

#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

namespace {

// Base64 parameter sets make fmtp longer than other text a plugin returns.
constexpr size_t kFmtpCapacity = 4096;
constexpr size_t kMaxPayloadHeader = 16;

ParameterSet FromPlugin(uint32_t set) {
    switch (set) {
        case GAZEORA_PARAMETER_SET_VPS:
            return ParameterSet::Vps;
        case GAZEORA_PARAMETER_SET_SPS:
            return ParameterSet::Sps;
        case GAZEORA_PARAMETER_SET_PPS:
            return ParameterSet::Pps;
        default:
            return ParameterSet::None;
    }
}

}  // namespace

PluginRtpPayloadFormat::PluginRtpPayloadFormat(const GazeoraCodecV2& codec)
    : m_plugin(codec),
      m_codec(PluginText::Narrow(codec.id)),
      m_encodingName(PluginText::Narrow(codec.rtp_encoding_name)),
      m_required(RequiredOf(codec)) {}

std::vector<ParameterSet> PluginRtpPayloadFormat::RequiredOf(const GazeoraCodecV2& codec) {
    std::vector<ParameterSet> required;
    for (uint32_t set : codec.required_parameter_sets) {
        if (FromPlugin(set) != ParameterSet::None) {
            required.push_back(FromPlugin(set));
        }
    }
    return required;
}

uint8_t PluginRtpPayloadFormat::NalType(const std::string& nal) const {
    return uint8_t(m_plugin.nal_type(m_plugin.self, Bytes(nal), nal.size()));
}

bool PluginRtpPayloadFormat::IsKeyFrame(const std::string& nal) const {
    return m_plugin.is_key_frame(m_plugin.self, Bytes(nal), nal.size()) == GAZEORA_TRUE;
}

ParameterSet PluginRtpPayloadFormat::ParameterSetOf(const std::string& nal) const {
    return FromPlugin(m_plugin.parameter_set_of(m_plugin.self, Bytes(nal), nal.size()));
}

std::string PluginRtpPayloadFormat::Fmtp(const std::string sets[4]) const {
    GazeoraBytesV2 plugin[GAZEORA_PARAMETER_SET_COUNT];
    for (uint32_t i = 0; i < GAZEORA_PARAMETER_SET_COUNT; ++i) {
        plugin[i] = {Bytes(sets[i]), sets[i].size()};
    }
    std::string fmtp(kFmtpCapacity, '\0');
    m_plugin.fmtp(m_plugin.self, plugin, &fmtp[0], fmtp.size());
    fmtp.resize(std::strlen(fmtp.c_str()));
    return fmtp;
}

std::string PluginRtpPayloadFormat::FragmentPayloadHeader(const std::string& nal) const {
    uint8_t header[kMaxPayloadHeader] = {};
    size_t size = m_plugin.fragment_payload_header(m_plugin.self, Bytes(nal), nal.size(), header,
                                                   sizeof(header));
    return std::string(reinterpret_cast<const char*>(header), std::min(size, sizeof(header)));
}

bool PluginRtpPayloadFormat::Depacketize(const uint8_t* payload, size_t size, bool& inFragment,
                                         std::string& annexB) const {
    if (!m_plugin.depacketize) {
        return false;
    }
    int32_t fragment = inFragment ? GAZEORA_TRUE : GAZEORA_FALSE;
    const bool ok = m_plugin.depacketize(m_plugin.self, payload, size, &fragment, Append,
                                         &annexB) == GAZEORA_TRUE;
    inFragment = fragment == GAZEORA_TRUE;
    return ok;
}

void GAZEORA_CALL PluginRtpPayloadFormat::Append(void* annexB, const uint8_t* data, size_t size) {
    if (annexB && (data || size == 0)) {
        static_cast<std::string*>(annexB)->append(reinterpret_cast<const char*>(data), size);
    }
}

}  // namespace gazeora
