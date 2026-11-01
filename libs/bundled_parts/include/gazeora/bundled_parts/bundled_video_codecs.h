// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/host/encode/video_codec_catalog.h"
#include "gazeora/host/encode/video_codec_info.h"
#include "gazeora/host/stream/video_codec_format.h"

namespace gazeora {

// The codec Gazeora's bundled plugins add (plugins/h265): H.265. Gazeora.exe gets it from the
// plugin; tests, tools and benchmarks that run without it get it here.
class BundledVideoCodecs final {
public:
    BundledVideoCodecs() = delete;

    static void RegisterInto(VideoCodecCatalog& catalog);
    // The built-in codec and the bundled one.
    static std::shared_ptr<const VideoCodecCatalog> Catalog();
    // The name of one of them, for labels outside the app (tools, benchmarks, tests).
    static std::string NameOf(const VideoCodec& codec);
    // The format of one of them, for the same. Throws std::invalid_argument for another codec.
    static const VideoCodecFormat& FormatOf(const VideoCodec& codec);
};

}  // namespace gazeora
