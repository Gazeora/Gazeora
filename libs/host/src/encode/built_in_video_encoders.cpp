// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/encode/built_in_video_encoders.h"

#include "gazeora/host/encode/media_foundation_encoder_provider.h"

namespace gazeora {

void BuiltInVideoEncoders::RegisterInto(VideoEncoderRegistry& registry) {
    registry.Register(std::make_shared<MediaFoundationEncoderProvider>());
}

std::shared_ptr<const VideoEncoderRegistry> BuiltInVideoEncoders::Registry() {
    auto registry = std::make_shared<VideoEncoderRegistry>();
    RegisterInto(*registry);
    return registry;
}

std::function<std::unique_ptr<VideoEncoder>()> BuiltInVideoEncoders::Factory() {
    std::shared_ptr<const VideoEncoderRegistry> registry = Registry();
    return [registry] { return registry->Create(); };
}

}  // namespace gazeora
