// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <map>
#include <memory>

#include "gazeora/host/encode/video_codec.h"
#include "gazeora/host/stream/video_codec_format.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_kit/exports/exported_codec.h"

namespace gazeora {

// The GazeoraCodecV2 of each codec the frames for one plugin output come in. Each is made on
// first use and kept while the cache lives, so the pointer the output was handed stays valid.
// Used from one thread.
class ExportedCodecCache final {
public:
    // `format` must outlive the cache: the formats in a codec catalog do.
    const GazeoraCodecV2& For(const VideoCodec& codec, const VideoCodecFormat& format);

private:
    std::map<const VideoCodecFormat*, std::unique_ptr<ExportedCodec>> m_codecs;
};

}  // namespace gazeora
