// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "adapters/plugin_encoder_provider.h"

#include "adapters/plugin_codec.h"
#include "adapters/plugin_video_encoder.h"
#include "adapters/refused_video_encoder.h"
#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

PluginEncoderProvider::PluginEncoderProvider(std::shared_ptr<const IPluginModule> module,
                                             const GazeoraEncoderProviderV2& provider)
    : m_module(std::move(module)),
      m_provider(provider),
      m_name(PluginText::Wide(provider.v1.name)) {}

bool PluginEncoderProvider::Supports(VideoCodec codec) const {
    if (m_provider.supports_id) {
        return m_provider.supports_id(m_provider.v1.self, codec.Id().c_str()) == GAZEORA_TRUE;
    }
    const std::optional<uint32_t> id = PluginCodec::IdOf(codec);
    return id && m_provider.v1.supports(m_provider.v1.self, *id) == GAZEORA_TRUE;
}

std::unique_ptr<VideoEncoder> PluginEncoderProvider::Create() const {
    GazeoraEncoderV1 encoder = {};
    encoder.struct_size = sizeof(encoder);
    if (m_provider.v1.create(m_provider.v1.self, &encoder) != GAZEORA_TRUE) {
        return std::make_unique<RefusedVideoEncoder>(m_name, L"could not create an encoder");
    }
    if (!PluginVideoEncoder::Valid(encoder)) {
        if (encoder.destroy) {
            encoder.destroy(encoder.self);
        }
        return std::make_unique<RefusedVideoEncoder>(m_name, L"made an incomplete encoder");
    }
    return std::make_unique<PluginVideoEncoder>(m_module, m_name, encoder,
                                                m_provider.supports_id != nullptr);
}

}  // namespace gazeora
