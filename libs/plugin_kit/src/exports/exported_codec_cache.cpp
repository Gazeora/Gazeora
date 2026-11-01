// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/exports/exported_codec_cache.h"

namespace gazeora {

const GazeoraCodecV2& ExportedCodecCache::For(const VideoCodec& codec,
                                              const VideoCodecFormat& format) {
    std::unique_ptr<ExportedCodec>& exported = m_codecs[&format];
    if (!exported) {
        // Borrowed, not owned: the catalog the format came from keeps it.
        std::shared_ptr<const VideoCodecFormat> borrowed(std::shared_ptr<const void>(), &format);
        VideoCodecInfo info{codec, codec.Id(), codec.Id()};
        info.format = std::move(borrowed);
        exported = std::make_unique<ExportedCodec>(std::move(info));
    }
    return exported->Codec();
}

}  // namespace gazeora
