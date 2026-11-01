// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/encode/bundled_video_codecs.h"

#include <stdexcept>

#include "gazeora/host/encode/built_in_video_codecs.h"
#include "gazeora/host/stream/codecs/h265_format.h"

namespace gazeora {

namespace {

// H.265 gets about the same picture as H.264 from this share of its bits.
constexpr uint32_t kH265BitratePercent = 70;

}  // namespace

VideoCodecInfo BundledVideoCodecs::H265() {
    return {VideoCodec::H265, "H.265", "H.265 - sharper on weak Wi-Fi, newer devices only",
            kH265BitratePercent, H265Format::Shared()};
}

void BundledVideoCodecs::RegisterInto(VideoCodecCatalog& catalog) { catalog.Register(H265()); }

std::shared_ptr<const VideoCodecCatalog> BundledVideoCodecs::Catalog() {
    auto catalog = std::make_shared<VideoCodecCatalog>();
    BuiltInVideoCodecs::RegisterInto(*catalog);
    RegisterInto(*catalog);
    return catalog;
}

std::string BundledVideoCodecs::NameOf(const VideoCodec& codec) { return Catalog()->NameOf(codec); }

const VideoCodecFormat& BundledVideoCodecs::FormatOf(const VideoCodec& codec) {
    // The formats are shared for the program's life, so the reference outlives the catalog it
    // came from.
    const VideoCodecInfo* info = Catalog()->Find(codec);
    if (!info || !info->format) {
        throw std::invalid_argument("not a built-in or bundled codec: " + codec.Id());
    }
    return *info->format;
}

}  // namespace gazeora
