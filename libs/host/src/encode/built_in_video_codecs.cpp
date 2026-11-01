// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/encode/built_in_video_codecs.h"

#include "gazeora/host/stream/codecs/h264_format.h"

namespace gazeora {

namespace {

constexpr uint32_t kFullBitratePercent = 100;

}  // namespace

VideoCodecInfo BuiltInVideoCodecs::H264() {
    return {VideoCodec::H264, "H.264", "H.264 - works on every device", kFullBitratePercent,
            H264Format::Shared()};
}

void BuiltInVideoCodecs::RegisterInto(VideoCodecCatalog& catalog) { catalog.Register(H264()); }

std::shared_ptr<const VideoCodecCatalog> BuiltInVideoCodecs::Catalog() {
    auto catalog = std::make_shared<VideoCodecCatalog>();
    RegisterInto(*catalog);
    return catalog;
}

}  // namespace gazeora
