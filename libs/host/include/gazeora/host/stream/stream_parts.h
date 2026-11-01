// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/host/encode/video_codec_catalog.h"
#include "gazeora/host/encode/video_encoder_registry.h"
#include "gazeora/host/stream/frame_output_registry.h"
#include "gazeora/host/stream/stream_protocol_catalog.h"

namespace gazeora {

// What a FrameStreamServer streams with: the video encoders, the outputs of the protocols other
// than RTSP, and the protocols and codecs it offers. Each one is shared and never changed:
// new parts replace them whole (FrameStreamServer::SetParts).
struct StreamParts {
    std::shared_ptr<const VideoEncoderRegistry> encoders;
    std::shared_ptr<const FrameOutputRegistry> outputs;
    std::shared_ptr<const StreamProtocolCatalog> protocols;
    std::shared_ptr<const VideoCodecCatalog> codecs;
};

}  // namespace gazeora
