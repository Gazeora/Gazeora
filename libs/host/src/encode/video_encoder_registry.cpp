// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/encode/video_encoder_registry.h"

#include <stdexcept>

#include "gazeora/host/encode/fallback_video_encoder.h"

namespace gazeora {

void VideoEncoderRegistry::Register(std::shared_ptr<const VideoEncoderProvider> provider) {
    if (!provider) {
        throw std::invalid_argument("VideoEncoderRegistry: null provider");
    }
    m_providers.push_back(std::move(provider));
}

bool VideoEncoderRegistry::Available(VideoCodec codec) const {
    for (const auto& provider : m_providers) {
        if (provider->Supports(codec)) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<VideoEncoder> VideoEncoderRegistry::Create() const {
    return std::make_unique<FallbackVideoEncoder>(m_providers);
}

}  // namespace gazeora
