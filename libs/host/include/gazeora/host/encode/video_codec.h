// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// A video codec, by its id: "h264", "h265", or another one a later Gazeora or a plugin adds.
// The id is what stream.ini and the Android app's discovery reply carry. Its name and how it's
// offered are in a VideoCodecCatalog (video_codec_catalog.h).
class VideoCodec final {
public:
    explicit VideoCodec(std::string id) : m_id(std::move(id)) {}

    static const VideoCodec H264;
    static const VideoCodec H265;  // HEVC: about a third fewer bits for the same picture

    const std::string& Id() const { return m_id; }
    bool operator==(const VideoCodec& other) const { return m_id == other.m_id; }
    bool operator!=(const VideoCodec& other) const { return m_id != other.m_id; }
    bool operator<(const VideoCodec& other) const { return m_id < other.m_id; }

private:
    std::string m_id;
};

inline const VideoCodec VideoCodec::H264{"h264"};
inline const VideoCodec VideoCodec::H265{"h265"};

}  // namespace gazeora
