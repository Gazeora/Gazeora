// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "file_start.h"

namespace frame_recorder {

namespace {

constexpr size_t kShortStartCode = 3;  // 00 00 01
constexpr size_t kLongStartCode = 4;   // 00 00 00 01

}  // namespace

GazeoraBytesV2 FileStart::Of(const GazeoraFrameV2& frame) {
    const GazeoraCodecV2* codec = frame.codec;
    size_t at = 0;
    while (codec && codec->is_access_unit_delimiter) {
        const size_t code = StartCodeSize(frame.annexb + at, frame.size - at);
        if (code == 0) {
            break;
        }
        const size_t nal = at + code;
        const size_t next = NextStartCode(frame.annexb, frame.size, nal);
        if (codec->is_access_unit_delimiter(codec->self, frame.annexb + nal, next - nal) !=
            GAZEORA_TRUE) {
            break;
        }
        at = next;
    }
    return {frame.annexb + at, frame.size - at};
}

size_t FileStart::StartCodeSize(const uint8_t* data, size_t size) {
    if (size >= kLongStartCode && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1) {
        return kLongStartCode;
    }
    if (size >= kShortStartCode && data[0] == 0 && data[1] == 0 && data[2] == 1) {
        return kShortStartCode;
    }
    return 0;
}

size_t FileStart::NextStartCode(const uint8_t* data, size_t size, size_t from) {
    for (size_t i = from; i + kShortStartCode <= size; ++i) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            return i > from && data[i - 1] == 0 ? i - 1 : i;  // a long start code's first 00
        }
    }
    return size;
}

}  // namespace frame_recorder
