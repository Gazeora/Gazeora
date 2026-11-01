// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/host/encode/video_codec.h"
#include "gazeora/host/encode/video_encoder.h"

namespace gazeora {

// Abstract Factory pattern: one way of encoding (Media Foundation, a vendor SDK...), registered
// in a VideoEncoderRegistry. Its encoders need what its documentation says on their thread.
class VideoEncoderProvider {
public:
    virtual ~VideoEncoderProvider() = default;

    // For logs, e.g. "Media Foundation".
    virtual const std::wstring& Name() const = 0;
    // Whether this PC has an encoder of this kind for `codec`. Cheap: starts none.
    virtual bool Supports(VideoCodec codec) const = 0;
    // A new, closed encoder.
    virtual std::unique_ptr<VideoEncoder> Create() const = 0;
};

}  // namespace gazeora
