// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/ts_muxer.h"

#include <cstring>

namespace gazeora {

namespace {

constexpr uint16_t kPatPid = 0x0000;
constexpr uint8_t kStreamTypeH264 = 0x1B;
constexpr uint8_t kStreamTypeH265 = 0x24;
constexpr uint8_t kVideoStreamId = 0xE0;
// The PTS runs this far ahead of the PCR: how long a player may hold a frame before showing
// it. Small, for a live game.
constexpr uint64_t kPtsDelay = 9000;  // 100 ms at 90 kHz

enum Continuity { kPat, kPmt, kVideo };

// Whether `annexB` starts with an access unit delimiter.
bool StartsWithDelimiter(VideoCodec codec, const std::string& annexB) {
    size_t at = annexB.find(std::string("\0\0\1", 3));
    if (at == std::string::npos || at + 3 >= annexB.size()) {
        return false;
    }
    uint8_t header = uint8_t(annexB[at + 3]);
    return codec == VideoCodec::H265 ? ((header >> 1) & 0x3F) == 35 : (header & 0x1F) == 9;
}

void PutTimestamp(uint8_t prefix, uint64_t value, std::string& out) {
    out += char(prefix | ((value >> 29) & 0x0E) | 1);
    out += char(value >> 22);
    out += char(((value >> 14) & 0xFE) | 1);
    out += char(value >> 7);
    out += char(((value << 1) & 0xFE) | 1);
}

}  // namespace

uint32_t Crc32Mpeg2(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; ++i) {
        crc ^= uint32_t(data[i]) << 24;
        for (int bit = 0; bit < 8; ++bit) {
            crc = crc & 0x80000000 ? (crc << 1) ^ 0x04C11DB7 : crc << 1;
        }
    }
    return crc;
}

void TsMuxer::Mux(VideoCodec codec, const std::string& annexB, uint32_t timestamp, bool keyFrame,
                  std::string& out) {
    if (m_started && timestamp < m_lastTimestamp && m_lastTimestamp - timestamp > 0x80000000u) {
        ++m_wraps;
    }
    m_started = true;
    m_lastTimestamp = timestamp;
    const uint64_t clock = ((m_wraps << 32) | timestamp) & 0x1FFFFFFFFull;
    const uint64_t pts = (clock + kPtsDelay) & 0x1FFFFFFFFull;

    if (keyFrame) {
        WriteTables(codec, out);
    }

    m_pes.clear();
    const char header[] = {0,          0, 1, char(kVideoStreamId),
                           0,          0,  // length: unbounded, as video PES packets may be
                           char(0x80),     // marker bits
                           char(0x80),     // PTS only
                           5};             // header data length
    m_pes.append(header, sizeof(header));
    PutTimestamp(0x20, pts, m_pes);
    if (!StartsWithDelimiter(codec, annexB)) {
        static const std::string kH264Delimiter("\0\0\0\1\x09\xF0", 6);
        static const std::string kH265Delimiter("\0\0\0\1\x46\x01\x50", 7);
        m_pes += codec == VideoCodec::H265 ? kH265Delimiter : kH264Delimiter;
    }
    m_pes += annexB;
    WritePes(m_pes, clock, keyFrame, out);
}

void TsMuxer::WriteTables(VideoCodec codec, std::string& out) {
    // The PMT's version changes with the codec, so a player that kept the old one rereads it.
    const uint8_t version = codec == VideoCodec::H265 ? 1 : 0;
    const uint8_t versionByte = uint8_t(0xC1 | (version << 1));

    uint8_t pat[] = {
        0x00,  // table: program association
        0xB0,
        13,  // section syntax, length
        0x00,
        0x01,  // transport stream id
        0xC1,
        0x00,
        0x00,  // version 0, current; section 0 of 0
        0x00,
        0x01,  // program 1
        uint8_t(0xE0 | (kPmtPid >> 8)),
        uint8_t(kPmtPid & 0xFF),
        0,
        0,
        0,
        0,  // CRC
    };
    uint32_t crc = Crc32Mpeg2(pat, sizeof(pat) - 4);
    for (int i = 0; i < 4; ++i) pat[sizeof(pat) - 4 + i] = uint8_t(crc >> (24 - 8 * i));
    WriteSection(kPatPid, pat, sizeof(pat), out);

    uint8_t pmt[] = {
        0x02,  // table: program map
        0xB0,
        18,  // section syntax, length
        0x00,
        0x01,  // program 1
        versionByte,
        0x00,
        0x00,  // section 0 of 0
        uint8_t(0xE0 | (kVideoPid >> 8)),
        uint8_t(kVideoPid & 0xFF),  // PCR on the video
        0xF0,
        0x00,  // no program info
        codec == VideoCodec::H265 ? kStreamTypeH265 : kStreamTypeH264,
        uint8_t(0xE0 | (kVideoPid >> 8)),
        uint8_t(kVideoPid & 0xFF),
        0xF0,
        0x00,  // no stream info
        0,
        0,
        0,
        0,  // CRC
    };
    crc = Crc32Mpeg2(pmt, sizeof(pmt) - 4);
    for (int i = 0; i < 4; ++i) pmt[sizeof(pmt) - 4 + i] = uint8_t(crc >> (24 - 8 * i));
    WriteSection(kPmtPid, pmt, sizeof(pmt), out);
}

void TsMuxer::WriteSection(uint16_t pid, const uint8_t* section, size_t size, std::string& out) {
    uint8_t& continuity = m_continuity[pid == kPatPid ? kPat : kPmt];
    uint8_t packet[kPacketSize];
    memset(packet, 0xFF, sizeof(packet));  // what's left of the payload is stuffing
    packet[0] = 0x47;
    packet[1] = uint8_t(0x40 | (pid >> 8));  // payload unit start
    packet[2] = uint8_t(pid);
    packet[3] = uint8_t(0x10 | (continuity++ & 0x0F));
    packet[4] = 0;  // pointer field: the section starts right away
    memcpy(packet + 5, section, size);
    out.append(reinterpret_cast<const char*>(packet), sizeof(packet));
}

void TsMuxer::WritePes(const std::string& pes, uint64_t pcrBase, bool keyFrame, std::string& out) {
    uint8_t& continuity = m_continuity[kVideo];
    size_t at = 0;
    bool first = true;
    while (at < pes.size()) {
        uint8_t packet[kPacketSize];
        packet[0] = 0x47;
        packet[1] = uint8_t((first ? 0x40 : 0) | (kVideoPid >> 8));
        packet[2] = uint8_t(kVideoPid & 0xFF);

        // The adaptation field, without its length byte: the PCR (and the random access flag)
        // on the first packet, stuffing on the last.
        uint8_t field[kPacketSize];
        size_t fieldSize = 0;
        bool hasField = false;
        if (first) {
            hasField = true;
            field[fieldSize++] = uint8_t(0x10 | (keyFrame ? 0x40 : 0));  // PCR, random access
            field[fieldSize++] = uint8_t(pcrBase >> 25);
            field[fieldSize++] = uint8_t(pcrBase >> 17);
            field[fieldSize++] = uint8_t(pcrBase >> 9);
            field[fieldSize++] = uint8_t(pcrBase >> 1);
            field[fieldSize++] = uint8_t(((pcrBase & 1) << 7) | 0x7E);  // reserved, extension 0
            field[fieldSize++] = 0;
        }
        size_t room = kPacketSize - 4 - (hasField ? 1 + fieldSize : 0);
        const size_t remaining = pes.size() - at;
        if (remaining < room) {
            size_t stuffing = room - remaining;
            if (!hasField) {
                hasField = true;
                --stuffing;  // the length byte
                if (stuffing > 0) {
                    field[fieldSize++] = 0x00;  // no flags
                    --stuffing;
                }
            }
            memset(field + fieldSize, 0xFF, stuffing);
            fieldSize += stuffing;
            room = remaining;
        }
        packet[3] = uint8_t((hasField ? 0x30 : 0x10) | (continuity++ & 0x0F));
        size_t offset = 4;
        if (hasField) {
            packet[offset++] = uint8_t(fieldSize);
            memcpy(packet + offset, field, fieldSize);
            offset += fieldSize;
        }
        memcpy(packet + offset, pes.data() + at, room);
        at += room;
        out.append(reinterpret_cast<const char*>(packet), sizeof(packet));
        first = false;
    }
}

}  // namespace gazeora
