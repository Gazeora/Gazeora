// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "adapters/plugin_video_encoder.h"

#include "adapters/plugin_codec.h"
#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

PluginVideoEncoder::PluginVideoEncoder(std::shared_ptr<const IPluginModule> module,
                                       std::wstring name, const GazeoraEncoderV1& encoder,
                                       bool knowsIds)
    : m_module(std::move(module)),
      m_name(std::move(name)),
      m_encoder(encoder),
      m_knowsIds(knowsIds) {}

PluginVideoEncoder::~PluginVideoEncoder() {
    Close();
    m_encoder.destroy(m_encoder.self);
}

bool PluginVideoEncoder::Valid(const GazeoraEncoderV1& encoder) {
    return encoder.struct_size >= sizeof(GazeoraEncoderV1) && encoder.open && encoder.close &&
           encoder.is_hardware && encoder.set_bitrate && encoder.request_key_frame &&
           encoder.encode && encoder.destroy;
}

bool PluginVideoEncoder::Open(const Settings& settings, std::wstring& error) {
    Close();
    if (!m_knowsIds && !PluginCodec::IdOf(settings.codec)) {
        return Failed({}, L"can't encode this codec", error);
    }
    if (settings.device && !m_encoder.encode_texture) {
        // Refused here, so the stream reads frames back to memory for it instead.
        return Failed({}, L"takes no textures", error);
    }
    GazeoraEncoderSettingsV2 pluginSettings = ToPlugin(settings);
    PluginText text;
    if (m_encoder.open(m_encoder.self, &pluginSettings.v1, text.Data(), text.Capacity()) !=
        GAZEORA_TRUE) {
        return Failed(text.Wide(), L"could not open", error);
    }
    m_settings = settings;
    m_device = settings.device;
    m_open = true;
    return true;
}

GazeoraEncoderSettingsV2 PluginVideoEncoder::ToPlugin(const Settings& settings) const {
    GazeoraEncoderSettingsV2 plugin = {};
    // A version 1 encoder is told it has the version 1 struct only.
    plugin.v1.struct_size = m_knowsIds ? sizeof(plugin) : sizeof(plugin.v1);
    plugin.v1.codec = PluginCodec::IdOf(settings.codec).value_or(GAZEORA_CODEC_OTHER);
    plugin.v1.width = settings.width;
    plugin.v1.height = settings.height;
    plugin.v1.fps = settings.fps;
    plugin.v1.bitrate = settings.bitrate;
    plugin.v1.key_interval = settings.keyInterval;
    plugin.v1.prefer_hardware = settings.preferHardware ? GAZEORA_TRUE : GAZEORA_FALSE;
    plugin.v1.d3d11_device = settings.device;
    plugin.codec_id = settings.codec.Id().c_str();
    return plugin;
}

void PluginVideoEncoder::Close() {
    if (m_open) {
        m_encoder.close(m_encoder.self);
        m_open = false;
    }
}

bool PluginVideoEncoder::IsHardware() const {
    return m_open && m_encoder.is_hardware(m_encoder.self) == GAZEORA_TRUE;
}

void PluginVideoEncoder::SetBitrate(uint32_t bitrate) {
    if (m_open) {
        m_encoder.set_bitrate(m_encoder.self, bitrate);
    }
}

void PluginVideoEncoder::RequestKeyFrame() {
    if (m_open) {
        m_encoder.request_key_frame(m_encoder.self);
    }
}

bool PluginVideoEncoder::Encode(const uint8_t* nv12, int64_t time, std::vector<Frame>& out,
                                std::wstring& error) {
    if (!m_open) {
        return Failed({}, L"is not open", error);
    }
    PluginText text;
    if (m_encoder.encode(m_encoder.self, nv12, time, CollectFrame, &out, text.Data(),
                         text.Capacity()) != GAZEORA_TRUE) {
        return Failed(text.Wide(), L"could not encode", error);
    }
    return true;
}

bool PluginVideoEncoder::EncodeTexture(ID3D11Texture2D* nv12, int64_t time, std::vector<Frame>& out,
                                       std::wstring& error) {
    if (!m_encoder.encode_texture) {
        return VideoEncoder::EncodeTexture(nv12, time, out, error);
    }
    if (!m_open) {
        return Failed({}, L"is not open", error);
    }
    PluginText text;
    if (m_encoder.encode_texture(m_encoder.self, nv12, time, CollectFrame, &out, text.Data(),
                                 text.Capacity()) != GAZEORA_TRUE) {
        return Failed(text.Wide(), L"could not encode", error);
    }
    return true;
}

void GAZEORA_CALL PluginVideoEncoder::CollectFrame(void* context,
                                                   const GazeoraEncodedFrameV1* frame) {
    if (!context || !frame || frame->struct_size < sizeof(GazeoraEncodedFrameV1) ||
        (!frame->annexb && frame->size > 0)) {
        return;
    }
    Frame copy;
    copy.annexB.assign(reinterpret_cast<const char*>(frame->annexb), frame->size);
    copy.time = frame->time;
    copy.keyFrame = frame->key_frame == GAZEORA_TRUE;
    static_cast<std::vector<Frame>*>(context)->push_back(std::move(copy));
}

bool PluginVideoEncoder::Failed(const std::wstring& text, const wchar_t* what,
                                std::wstring& error) const {
    error = m_name + L" " + what + (text.empty() ? L"" : L": " + text);
    return false;
}

}  // namespace gazeora
