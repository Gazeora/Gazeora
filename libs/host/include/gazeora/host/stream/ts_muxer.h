// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "gazeora/host/encode/video_codec.h"

namespace gazeora {

// Live H.264 or H.265 in an MPEG transport stream (ISO/IEC 13818-1), for the HTTP protocol:
// one program with one video stream. Every key frame comes after the PAT and PMT and carries
// the random access flag, so a player can join on any key frame. Each frame is one PES
// packet, with an access unit delimiter in front (transport streams require one) and the
// PCR on its first TS packet. Use from one thread.
class TsMuxer {
public:
    static constexpr size_t kPacketSize = 188;
    static constexpr uint16_t kPmtPid = 0x1000;
    static constexpr uint16_t kVideoPid = 0x0100;

    // Appends the TS packets of one access unit (Annex B) to `out`.
    void Mux(VideoCodec codec, const std::string& annexB, uint32_t timestamp, bool keyFrame,
             std::string& out);

private:
    void WriteTables(VideoCodec codec, std::string& out);
    void WriteSection(uint16_t pid, const uint8_t* section, size_t size, std::string& out);
    void WritePes(const std::string& pes, uint64_t pcrBase, bool keyFrame, std::string& out);

    uint8_t m_continuity[3] = {};  // PAT, PMT, video
    // The 90 kHz clock unwrapped past 32 bits, as PTS and PCR have 33.
    uint32_t m_lastTimestamp = 0;
    uint64_t m_wraps = 0;
    bool m_started = false;
    std::string m_pes;  // reused between frames
};

// CRC-32/MPEG-2, which PSI sections end with.
uint32_t Crc32Mpeg2(const uint8_t* data, size_t size);

}  // namespace gazeora
