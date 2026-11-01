// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/bundled_parts/bundled_video_codecs.h"

#include <stdexcept>

#include "gazeora/bundled/h265/h265_format.h"
#include "gazeora/host/encode/built_in_video_codecs.h"

namespace gazeora {

void BundledVideoCodecs::RegisterInto(VideoCodecCatalog& catalog) {
    catalog.Register(H265Format::Info());
}

std::shared_ptr<const VideoCodecCatalog> BundledVideoCodecs::Catalog() {
    auto catalog = std::make_shared<VideoCodecCatalog>();
    BuiltInVideoCodecs::RegisterInto(*catalog);
    RegisterInto(*catalog);
    return catalog;
}

namespace {

// One catalog for the lookups below, kept for the program's life: what Find returns points into
// it, so a catalog made for one call would be gone before its answer is read.
const VideoCodecCatalog& LookupCatalog() {
    static const std::shared_ptr<const VideoCodecCatalog> catalog = BundledVideoCodecs::Catalog();
    return *catalog;
}

}  // namespace

std::string BundledVideoCodecs::NameOf(const VideoCodec& codec) {
    return LookupCatalog().NameOf(codec);
}

const VideoCodecFormat& BundledVideoCodecs::FormatOf(const VideoCodec& codec) {
    const VideoCodecInfo* info = LookupCatalog().Find(codec);
    if (!info || !info->format) {
        throw std::invalid_argument("not a built-in or bundled codec: " + codec.Id());
    }
    return *info->format;
}

}  // namespace gazeora
