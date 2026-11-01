// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/host/encode/media_foundation_encoder_provider.h"
#include "gazeora/host/encode/video_encoder_registry.h"
#include "host/stream/software_only_encoder.h"

namespace gazeora {

// Decorator pattern: the Media Foundation provider, making SoftwareOnlyEncoders.
class SoftwareOnlyEncoderProvider final : public VideoEncoderProvider {
public:
    const std::wstring& Name() const override { return m_inner.Name(); }
    bool Supports(VideoCodec codec) const override { return m_inner.Supports(codec); }
    std::unique_ptr<VideoEncoder> Create() const override { return SoftwareOnlyEncoder::Create(); }

    // A registry with only this provider, for FrameStreamServer.
    static std::shared_ptr<const VideoEncoderRegistry> Registry() {
        auto registry = std::make_shared<VideoEncoderRegistry>();
        registry->Register(std::make_shared<SoftwareOnlyEncoderProvider>());
        return registry;
    }

private:
    MediaFoundationEncoderProvider m_inner;
};

}  // namespace gazeora
