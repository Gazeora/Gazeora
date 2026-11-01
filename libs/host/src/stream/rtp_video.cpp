// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/rtp_video.h"

#include <emmintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <algorithm>
#include <cstdio>

namespace gazeora {

namespace {

constexpr size_t kRtpHeaderSize = 12;
const char kStartCode[] = {0, 0, 0, 1};

void PutRtpHeader(std::string& packet, bool marker, uint16_t seq, uint32_t timestamp,
                  uint32_t ssrc) {
    packet += char(0x80);  // version 2
    packet += char((marker ? 0x80 : 0) | kRtpVideoPayloadType);
    for (int shift = 8; shift >= 0; shift -= 8) packet += char(seq >> shift);
    for (int shift = 24; shift >= 0; shift -= 8) packet += char(timestamp >> shift);
    for (int shift = 24; shift >= 0; shift -= 8) packet += char(ssrc >> shift);
}

class H264PayloadFormat final : public RtpPayloadFormat {
public:
    VideoCodec Codec() const override { return VideoCodec::H264; }
    const char* EncodingName() const override { return "H264"; }
    uint8_t NalType(const std::string& nal) const override {
        return nal.empty() ? 0 : nal[0] & 0x1F;
    }
    bool IsKeyFrame(const std::string& nal) const override { return NalType(nal) == kNalIdrSlice; }
    ParameterSet ParameterSetOf(const std::string& nal) const override {
        uint8_t type = NalType(nal);
        return type == kNalSps   ? ParameterSet::Sps
               : type == kNalPps ? ParameterSet::Pps
                                 : ParameterSet::None;
    }
    std::vector<ParameterSet> RequiredParameterSets() const override {
        return {ParameterSet::Sps, ParameterSet::Pps};
    }
    std::string Fmtp(const std::string sets[4]) const override {
        const std::string& sps = sets[int(ParameterSet::Sps)];
        const std::string& pps = sets[int(ParameterSet::Pps)];
        std::string fmtp = "packetization-mode=1";
        if (sps.size() >= 4) {
            char profile[32];
            snprintf(profile, sizeof(profile), ";profile-level-id=%02X%02X%02X", uint8_t(sps[1]),
                     uint8_t(sps[2]), uint8_t(sps[3]));
            fmtp += profile;
            if (!pps.empty()) {
                fmtp += ";sprop-parameter-sets=" + Base64(sps) + "," + Base64(pps);
            }
        }
        return fmtp;
    }
    bool Depacketize(const uint8_t* payload, size_t size, bool& inFragment,
                     std::string& annexB) const override {
        if (size < 1) return false;
        uint8_t type = payload[0] & 0x1F;
        if (type >= 1 && type <= 23) {
            inFragment = false;
            AppendNal(payload, size, annexB);
            return true;
        }
        if (type == kNalStapA) {
            inFragment = false;
            return UnpackAggregate(payload, size, 1, annexB);
        }
        if (type == kNalFuA && size >= 2) {
            char header = char((payload[0] & 0xE0) | (payload[1] & 0x1F));
            return AddFragment(payload, size, 2, payload[1], &header, 1, inFragment, annexB);
        }
        return false;
    }

protected:
    size_t NalHeaderSize() const override { return 1; }
    // FU-A: indicator (F, NRI, 28).
    std::string FragmentPayloadHeader(const std::string& nal) const override {
        return std::string(1, char((nal[0] & 0xE0) | kNalFuA));
    }
};

}  // namespace

std::string RtpPayloadFormat::Base64(const std::string& data) {
    static const char kDigits[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string text;
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t chunk = uint32_t(uint8_t(data[i])) << 16;
        if (i + 1 < data.size()) chunk |= uint32_t(uint8_t(data[i + 1])) << 8;
        if (i + 2 < data.size()) chunk |= uint8_t(data[i + 2]);
        text += kDigits[chunk >> 18 & 63];
        text += kDigits[chunk >> 12 & 63];
        text += i + 1 < data.size() ? kDigits[chunk >> 6 & 63] : '=';
        text += i + 2 < data.size() ? kDigits[chunk & 63] : '=';
    }
    return text;
}

bool RtpPayloadFormat::UnpackAggregate(const uint8_t* payload, size_t size, size_t headerSize,
                                       std::string& annexB) {
    for (size_t at = headerSize; at + 2 <= size;) {
        size_t length = size_t(payload[at]) << 8 | payload[at + 1];
        at += 2;
        if (at + length > size) return false;
        annexB.append(kStartCode, 4);
        annexB.append(reinterpret_cast<const char*>(payload) + at, length);
        at += length;
    }
    return true;
}

bool RtpPayloadFormat::AddFragment(const uint8_t* payload, size_t size, size_t dataAt,
                                   uint8_t fragmentHeader, const char* header, size_t headerSize,
                                   bool& inFragment, std::string& annexB) {
    bool start = fragmentHeader & 0x80;
    bool end = fragmentHeader & 0x40;
    if (start) {
        annexB.append(kStartCode, 4);
        annexB.append(header, headerSize);
        inFragment = true;
    } else if (!inFragment) {
        return false;
    }
    annexB.append(reinterpret_cast<const char*>(payload) + dataAt, size - dataAt);
    if (end) inFragment = false;
    return true;
}

void RtpPayloadFormat::AppendNal(const uint8_t* payload, size_t size, std::string& annexB) {
    annexB.append(kStartCode, 4);
    annexB.append(reinterpret_cast<const char*>(payload), size);
}

const RtpPayloadFormat& RtpPayloadFormat::H264() {
    static const H264PayloadFormat format;
    return format;
}

void RtpPayloadFormat::Packetize(const std::vector<std::string>& nalUnits, uint32_t timestamp,
                                 uint32_t ssrc, size_t maxPacketSize, uint16_t& seq,
                                 std::vector<std::string>& packets) const {
    // The packets' strings are kept from the last frame and written over, so a stream doesn't
    // allocate a packet's buffer once it has had one that size.
    size_t used = 0;
    auto next = [&]() -> std::string& {
        if (used == packets.size()) {
            packets.emplace_back().reserve(maxPacketSize);
        }
        std::string& packet = packets[used++];
        packet.clear();
        return packet;
    };
    const size_t headerSize = NalHeaderSize();
    const size_t maxPayload = maxPacketSize - kRtpHeaderSize;
    for (size_t n = 0; n < nalUnits.size(); ++n) {
        const std::string& nal = nalUnits[n];
        if (nal.size() < headerSize) continue;
        bool lastNal = n + 1 == nalUnits.size();
        if (nal.size() <= maxPayload) {
            std::string& packet = next();
            PutRtpHeader(packet, lastNal, seq++, timestamp, ssrc);
            packet += nal;
            continue;
        }
        // The NAL header moves into the payload header, plus a fragment header with the
        // start and end bits and the original type; the rest of the NAL unit is spread over
        // as many packets as it needs.
        const std::string payloadHeader = FragmentPayloadHeader(nal);
        const uint8_t type = NalType(nal);
        const size_t chunkSize = maxPayload - payloadHeader.size() - 1;
        for (size_t offset = headerSize; offset < nal.size(); offset += chunkSize) {
            bool first = offset == headerSize;
            bool last = offset + chunkSize >= nal.size();
            std::string& packet = next();
            PutRtpHeader(packet, lastNal && last, seq++, timestamp, ssrc);
            packet += payloadHeader;
            packet += char((first ? 0x80 : 0) | (last ? 0x40 : 0) | type);
            packet.append(nal, offset, chunkSize);
        }
    }
    packets.resize(used);
}

namespace {

// The position of the first zero byte in [from, end), or `end`. Start codes begin with zeros,
// which are rare in coded slices, so this skips 16 bytes per step with SSE2.
size_t FindZero(const char* data, size_t from, size_t end) {
    const __m128i zero = _mm_setzero_si128();
    for (; from + 16 <= end; from += 16) {
        __m128i bytes = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + from));
        unsigned mask = unsigned(_mm_movemask_epi8(_mm_cmpeq_epi8(bytes, zero)));
        if (mask) {
#ifdef _MSC_VER
            unsigned long bit;
            _BitScanForward(&bit, mask);
            return from + bit;
#else
            return from + unsigned(__builtin_ctz(mask));
#endif
        }
    }
    for (; from < end && data[from] != 0; ++from) {
    }
    return from;
}

}  // namespace

void SplitAnnexB(const std::string& annexB, std::vector<std::string>& nalUnits) {
    nalUnits.clear();
    size_t start = std::string::npos;
    const size_t size = annexB.size();
    for (size_t i = FindZero(annexB.data(), 0, size); i + 2 < size;
         i = FindZero(annexB.data(), i, size)) {
        if (annexB[i + 1] == 0 && annexB[i + 2] == 1) {
            if (start != std::string::npos) {
                // A four-byte start code leaves a zero behind the previous NAL unit.
                size_t end = i > start && annexB[i - 1] == 0 ? i - 1 : i;
                nalUnits.push_back(annexB.substr(start, end - start));
            }
            start = i + 3;
            i += 3;
        } else {
            ++i;
        }
    }
    if (start != std::string::npos && start < size) {
        nalUnits.push_back(annexB.substr(start));
    }
}

bool ParameterSetCache::Update(const std::vector<std::string>& nalUnits) {
    bool changed = false;
    for (const std::string& nal : nalUnits) {
        ParameterSet set = m_format->ParameterSetOf(nal);
        if (set != ParameterSet::None && m_sets[int(set)] != nal) {
            m_sets[int(set)] = nal;
            changed = true;
        }
    }
    return changed;
}

void ParameterSetCache::PrependTo(std::vector<std::string>& nalUnits) const {
    bool carriesSps = std::any_of(nalUnits.begin(), nalUnits.end(), [&](const std::string& nal) {
        return m_format->ParameterSetOf(nal) == ParameterSet::Sps;
    });
    if (carriesSps || !HasAll()) {
        return;
    }
    std::vector<std::string> sets;
    for (ParameterSet set : m_format->RequiredParameterSets()) {
        sets.push_back(m_sets[int(set)]);
    }
    nalUnits.insert(nalUnits.begin(), sets.begin(), sets.end());
}

bool ParameterSetCache::HasAll() const {
    for (ParameterSet set : m_format->RequiredParameterSets()) {
        if (m_sets[int(set)].empty()) return false;
    }
    return true;
}

void ParameterSetCache::Clear() {
    for (std::string& set : m_sets) set.clear();
}

}  // namespace gazeora
