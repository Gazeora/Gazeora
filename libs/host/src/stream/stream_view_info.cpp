#include "gazeora/host/stream/stream_view_info.h"

#include <cstdio>

namespace gazeora {

namespace {

constexpr uint8_t kUserDataUnregistered = 5;  // SEI payload type
constexpr uint8_t kH264Sei = 6;               // NAL unit types
constexpr uint8_t kH265PrefixSei = 39;

// Inserts emulation prevention bytes: no 00 00 followed by 00..03 inside a NAL unit.
std::string Escape(const std::string& payload) {
    std::string escaped;
    escaped.reserve(payload.size() + 4);
    int zeros = 0;
    for (char c : payload) {
        uint8_t byte = uint8_t(c);
        if (zeros >= 2 && byte <= 3) {
            escaped += '\x03';
            zeros = 0;
        }
        escaped += c;
        zeros = byte == 0 ? zeros + 1 : 0;
    }
    return escaped;
}

}  // namespace

std::string ViewInfoText(const VrLayout& vr, bool pair) {
    if (!vr.enabled || !vr.lensOnPhone) {
        return {};
    }
    char text[160];
    snprintf(text, sizeof(text), "vr=1 pair=%d k1=%.3f k2=%.3f scale=%.3f sep=%.3f chroma=%.3f",
             pair ? 1 : 0, vr.k1, vr.k2, vr.imageScale, vr.lensSeparation, vr.chromaCorrection);
    return text;
}

std::string ViewInfoNal(VideoCodec codec, const std::string& text) {
    std::string message(kViewInfoId, 16);
    message += text;
    std::string sei;
    sei += char(kUserDataUnregistered);
    size_t size = message.size();
    for (; size >= 255; size -= 255) sei += '\xFF';
    sei += char(size);
    sei += message;
    sei += '\x80';  // rbsp trailing bits

    std::string nal;
    if (codec == VideoCodec::H265) {
        nal += char(kH265PrefixSei << 1);
        nal += '\x01';  // layer 0, temporal id 0 (+1)
    } else {
        nal += char(kH264Sei);
    }
    return nal + Escape(sei);
}

bool IsPictureNal(VideoCodec codec, const std::string& nal) {
    if (nal.empty()) {
        return false;
    }
    uint8_t first = uint8_t(nal[0]);
    if (codec == VideoCodec::H265) {
        return (first >> 1 & 0x3F) < 32;  // VCL types are 0..31
    }
    uint8_t type = first & 0x1F;
    return type >= 1 && type <= 5;
}

}  // namespace gazeora
