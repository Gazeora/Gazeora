// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <optional>

#include "gazeora/host/encode/video_codec.h"
#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// VideoCodec as plugin API version 1 numbers it (GAZEORA_CODEC_*), which has H.264 and
// H.265 only.
class PluginCodec final {
public:
    PluginCodec() = delete;

    // Nothing for a codec version 1 has no number for.
    static std::optional<uint32_t> IdOf(const VideoCodec& codec) {
        if (codec == VideoCodec::H264) {
            return GAZEORA_CODEC_H264;
        }
        if (codec == VideoCodec::H265) {
            return GAZEORA_CODEC_H265;
        }
        return std::nullopt;
    }
};

}  // namespace gazeora
