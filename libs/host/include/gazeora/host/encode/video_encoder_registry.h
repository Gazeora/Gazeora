// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <vector>

#include "gazeora/host/encode/video_encoder_provider.h"

namespace gazeora {

// Registry pattern: the ways this build can encode, most preferred first. Fill it before
// handing it out; it's read only after that.
class VideoEncoderRegistry final {
public:
    // Added after the ones already registered. Throws std::invalid_argument for null.
    void Register(std::shared_ptr<const VideoEncoderProvider> provider);

    // Whether any provider has an encoder for `codec` on this PC.
    bool Available(VideoCodec codec) const;
    // A closed encoder that opens with the first provider that manages to
    // (FallbackVideoEncoder).
    std::unique_ptr<VideoEncoder> Create() const;

private:
    std::vector<std::shared_ptr<const VideoEncoderProvider>> m_providers;
};

}  // namespace gazeora
