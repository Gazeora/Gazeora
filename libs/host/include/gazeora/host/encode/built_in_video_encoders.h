// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <functional>
#include <memory>

#include "gazeora/host/encode/video_encoder_registry.h"

namespace gazeora {

// The encoders Gazeora ships: Media Foundation.
class BuiltInVideoEncoders final {
public:
    BuiltInVideoEncoders() = delete;

    static void RegisterInto(VideoEncoderRegistry& registry);
    // A registry with only the built-in encoders.
    static std::shared_ptr<const VideoEncoderRegistry> Registry();
    // Makes encoders from Registry(), for VideoPipeline.
    static std::function<std::unique_ptr<VideoEncoder>()> Factory();
};

}  // namespace gazeora
