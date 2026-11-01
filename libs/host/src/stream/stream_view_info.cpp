// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/stream_view_info.h"

#include <cstdio>

namespace gazeora {

namespace {

constexpr uint8_t kUserDataUnregistered = 5;  // SEI payload type

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

std::string ViewInfoText(const VrLayout& vr, bool pair, FrameSource source) {
    if (!vr.enabled || !vr.lensOnPhone) {
        return {};
    }
    char text[160];
    snprintf(text, sizeof(text), "vr=1 pair=%d k1=%.3f k2=%.3f scale=%.3f sep=%.3f chroma=%.3f%s",
             pair ? 1 : 0, vr.k1, vr.k2, vr.imageScale, vr.lensSeparation, vr.chromaCorrection,
             source == FrameSource::App ? " src=app" : "");
    return text;
}

std::string ViewInfoNal(const VideoCodecFormat& format, const std::string& text) {
    std::string message(kViewInfoId, 16);
    message += text;
    std::string sei;
    sei += char(kUserDataUnregistered);
    size_t size = message.size();
    for (; size >= 255; size -= 255) sei += '\xFF';
    sei += char(size);
    sei += message;
    sei += '\x80';  // rbsp trailing bits

    return format.SeiHeader() + Escape(sei);
}

}  // namespace gazeora
