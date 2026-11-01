// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/host/encode/video_codec_catalog.h"
#include "gazeora/host/encode/video_codec_info.h"

namespace gazeora {

// The codec Gazeora.exe has itself: H.264, which every device decodes and the stream falls
// back to. The others come from plugins (bundled_video_codecs.h).
class BuiltInVideoCodecs final {
public:
    BuiltInVideoCodecs() = delete;

    static VideoCodecInfo H264();
    static void RegisterInto(VideoCodecCatalog& catalog);
    // A catalog with only H.264.
    static std::shared_ptr<const VideoCodecCatalog> Catalog();
};

}  // namespace gazeora
