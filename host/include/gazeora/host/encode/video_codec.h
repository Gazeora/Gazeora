#pragma once

namespace gazeora {

enum class VideoCodec {
    H264,
    H265,  // HEVC: about a third fewer bits for the same picture, but newer players only
};

inline const char* CodecName(VideoCodec codec) {
    return codec == VideoCodec::H265 ? "H.265" : "H.264";
}

}  // namespace gazeora
