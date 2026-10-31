// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/frame_packet.h"

#include <algorithm>
#include <cstring>

namespace gazeora {

namespace {

constexpr char kMagic[4] = {'G', 'Z', 'F', '1'};

void Put16(uint8_t* out, uint16_t value) {
    out[0] = uint8_t(value >> 8);
    out[1] = uint8_t(value);
}

void Put32(uint8_t* out, uint32_t value) {
    out[0] = uint8_t(value >> 24);
    out[1] = uint8_t(value >> 16);
    out[2] = uint8_t(value >> 8);
    out[3] = uint8_t(value);
}

uint16_t Get16(const uint8_t* in) { return uint16_t(in[0] << 8 | in[1]); }

uint32_t Get32(const uint8_t* in) {
    return uint32_t(in[0]) << 24 | uint32_t(in[1]) << 16 | uint32_t(in[2]) << 8 | in[3];
}

}  // namespace

void FrameHeader::Write(uint8_t out[kFrameHeaderSize]) const {
    memcpy(out, kMagic, 4);
    out[4] = uint8_t(type);
    out[5] = flags;
    Put16(out + 6, width);
    Put16(out + 8, height);
    Put16(out + 10, lossPerMille);
    Put32(out + 12, frameNumber);
    Put32(out + 16, timestamp);
    Put32(out + 20, tail);
}

bool FrameHeader::Read(const uint8_t* data, size_t size) {
    if (size < kFrameHeaderSize || memcmp(data, kMagic, 4) != 0) {
        return false;
    }
    type = FrameMessage(data[4]);
    flags = data[5];
    width = Get16(data + 6);
    height = Get16(data + 8);
    lossPerMille = Get16(data + 10);
    frameNumber = Get32(data + 12);
    timestamp = Get32(data + 16);
    tail = Get32(data + 20);
    return true;
}

void ChunkFrame(FrameHeader header, const std::string& annexB,
                std::vector<std::string>& datagrams) {
    constexpr size_t kChunk = kMaxFrameDatagram - kFrameHeaderSize;
    const size_t count = std::max<size_t>(1, (annexB.size() + kChunk - 1) / kChunk);
    datagrams.resize(count);
    uint8_t raw[kFrameHeaderSize];
    for (size_t i = 0; i < count; ++i) {
        header.tail = uint32_t(i) << 16 | uint32_t(count);
        header.Write(raw);
        size_t at = i * kChunk;
        size_t size = std::min(kChunk, annexB.size() - std::min(at, annexB.size()));
        std::string& datagram = datagrams[i];
        datagram.assign(reinterpret_cast<const char*>(raw), kFrameHeaderSize);
        datagram.append(annexB, at, size);
    }
}

std::string StreamMessage(FrameHeader header, const std::string& payload) {
    header.tail = uint32_t(payload.size());
    uint8_t raw[kFrameHeaderSize];
    header.Write(raw);
    std::string message(reinterpret_cast<const char*>(raw), kFrameHeaderSize);
    message += payload;
    return message;
}

}  // namespace gazeora
