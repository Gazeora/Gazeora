#include "gazeora/host/stream/rtp_video.h"

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

std::string Base64(const std::string& data) {
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

// Aggregation packets (H.264 STAP-A, H.265 AP): after the payload header, NAL units each
// with a 16-bit size.
bool UnpackAggregate(const uint8_t* payload, size_t size, size_t headerSize, std::string& annexB) {
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

// Fragmentation units: the start fragment rebuilds the NAL header from `header` (its
// `headerSize` bytes), every fragment adds its data after `dataAt`.
bool AddFragment(const uint8_t* payload, size_t size, size_t dataAt, uint8_t fragmentHeader,
                 const char* header, size_t headerSize, bool& inFragment, std::string& annexB) {
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

class H264PayloadFormat final : public RtpPayloadFormat {
public:
    VideoCodec Codec() const override { return VideoCodec::H264; }
    const char* EncodingName() const override { return "H264"; }
    uint8_t NalType(const std::string& nal) const override {
        return nal.empty() ? 0 : nal[0] & 0x1F;
    }
    bool IsKeyFrame(const std::string& nal) const override {
        return NalType(nal) == kNalIdrSlice;
    }
    ParameterSet ParameterSetOf(const std::string& nal) const override {
        uint8_t type = NalType(nal);
        return type == kNalSps ? ParameterSet::Sps
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
            snprintf(profile, sizeof(profile), ";profile-level-id=%02X%02X%02X",
                     uint8_t(sps[1]), uint8_t(sps[2]), uint8_t(sps[3]));
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
            annexB.append(kStartCode, 4);
            annexB.append(reinterpret_cast<const char*>(payload), size);
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

class H265PayloadFormat final : public RtpPayloadFormat {
public:
    VideoCodec Codec() const override { return VideoCodec::H265; }
    const char* EncodingName() const override { return "H265"; }
    uint8_t NalType(const std::string& nal) const override {
        return nal.empty() ? 0 : uint8_t(nal[0]) >> 1 & 0x3F;
    }
    bool IsKeyFrame(const std::string& nal) const override {
        uint8_t type = NalType(nal);
        return type >= kHevcIrapFirst && type <= kHevcIrapLast;
    }
    ParameterSet ParameterSetOf(const std::string& nal) const override {
        uint8_t type = NalType(nal);
        return type == kHevcVps   ? ParameterSet::Vps
               : type == kHevcSps ? ParameterSet::Sps
               : type == kHevcPps ? ParameterSet::Pps
                                  : ParameterSet::None;
    }
    std::vector<ParameterSet> RequiredParameterSets() const override {
        return {ParameterSet::Vps, ParameterSet::Sps, ParameterSet::Pps};
    }
    std::string Fmtp(const std::string sets[4]) const override {
        const std::string& vps = sets[int(ParameterSet::Vps)];
        const std::string& sps = sets[int(ParameterSet::Sps)];
        const std::string& pps = sets[int(ParameterSet::Pps)];
        if (vps.empty() || sps.empty() || pps.empty()) {
            return {};
        }
        return "sprop-vps=" + Base64(vps) + ";sprop-sps=" + Base64(sps) +
               ";sprop-pps=" + Base64(pps);
    }
    bool Depacketize(const uint8_t* payload, size_t size, bool& inFragment,
                     std::string& annexB) const override {
        if (size < 2) return false;
        uint8_t type = payload[0] >> 1 & 0x3F;
        if (type < kHevcAp) {
            inFragment = false;
            annexB.append(kStartCode, 4);
            annexB.append(reinterpret_cast<const char*>(payload), size);
            return true;
        }
        if (type == kHevcAp) {  // without DONL fields: the SDP sets no sprop-max-don-diff
            inFragment = false;
            return UnpackAggregate(payload, size, 2, annexB);
        }
        if (type == kHevcFu && size >= 3) {
            char header[2] = {char((payload[0] & 0x81) | (payload[2] & 0x3F) << 1),
                              char(payload[1])};
            return AddFragment(payload, size, 3, payload[2], header, 2, inFragment, annexB);
        }
        return false;
    }

protected:
    size_t NalHeaderSize() const override { return 2; }
    // FU: the two-byte NAL header with type 49, keeping the layer and temporal id.
    std::string FragmentPayloadHeader(const std::string& nal) const override {
        return {char((nal[0] & 0x81) | kHevcFu << 1), nal[1]};
    }
};

}  // namespace

const RtpPayloadFormat& RtpPayloadFormat::For(VideoCodec codec) {
    static const H264PayloadFormat h264;
    static const H265PayloadFormat h265;
    return codec == VideoCodec::H265 ? static_cast<const RtpPayloadFormat&>(h265) : h264;
}

void RtpPayloadFormat::Packetize(const std::vector<std::string>& nalUnits, uint32_t timestamp,
                                 uint32_t ssrc, size_t maxPacketSize, uint16_t& seq,
                                 std::vector<std::string>& packets) const {
    packets.clear();
    const size_t headerSize = NalHeaderSize();
    const size_t maxPayload = maxPacketSize - kRtpHeaderSize;
    for (size_t n = 0; n < nalUnits.size(); ++n) {
        const std::string& nal = nalUnits[n];
        if (nal.size() < headerSize) continue;
        bool lastNal = n + 1 == nalUnits.size();
        if (nal.size() <= maxPayload) {
            std::string packet;
            packet.reserve(kRtpHeaderSize + nal.size());
            PutRtpHeader(packet, lastNal, seq++, timestamp, ssrc);
            packets.push_back(packet + nal);
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
            std::string packet;
            packet.reserve(maxPacketSize);
            PutRtpHeader(packet, lastNal && last, seq++, timestamp, ssrc);
            packet += payloadHeader;
            packet += char((first ? 0x80 : 0) | (last ? 0x40 : 0) | type);
            packet.append(nal, offset, chunkSize);
            packets.push_back(std::move(packet));
        }
    }
}

void SplitAnnexB(const std::string& annexB, std::vector<std::string>& nalUnits) {
    nalUnits.clear();
    size_t start = std::string::npos;
    for (size_t i = 0; i + 2 < annexB.size(); ++i) {
        if (annexB[i] == 0 && annexB[i + 1] == 0 && annexB[i + 2] == 1) {
            if (start != std::string::npos) {
                // A four-byte start code leaves a zero behind the previous NAL unit.
                size_t end = i > start && annexB[i - 1] == 0 ? i - 1 : i;
                nalUnits.push_back(annexB.substr(start, end - start));
            }
            start = i + 3;
            i += 2;
        }
    }
    if (start != std::string::npos && start < annexB.size()) {
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
