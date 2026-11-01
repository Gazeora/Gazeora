// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "gazeora/host/encode/video_codec.h"

namespace gazeora {

class VideoCodecFormat;  // video_codec_format.h, in the stream module

// What the UI and the stream server need to know about a codec.
struct VideoCodecInfo {
    VideoCodec id;
    std::string name;    // for people, e.g. "H.264"
    std::string choice;  // its line in the settings' codec list
    // The share of the full-quality bitrate it needs for about the same picture as H.264.
    uint32_t bitratePercent = 100;
    // How the stream carries it (video_codec_format.h). Never null in a catalog in use.
    std::shared_ptr<const VideoCodecFormat> format;
};

}  // namespace gazeora
