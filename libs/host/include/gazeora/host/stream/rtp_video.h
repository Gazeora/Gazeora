// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/encode/video_codec.h"

namespace gazeora {

// Video over RTP, one class per codec: H.264 here (RFC 6184, packetization mode 1), others in
// their plugins, such as H.265 (plugins/h265). A NAL unit that fits goes in a packet of its own,
// a larger one is cut into fragmentation units. Everything that differs between the codecs lives
// behind RtpPayloadFormat; a codec's VideoCodecFormat (video_codec_format.h) says which one it
// uses.

constexpr uint8_t kRtpVideoPayloadType = 96;  // dynamic, declared in the SDP

// H.264 NAL unit types (the low 5 bits of the one-byte header).
enum H264NalType : uint8_t {
    kNalSlice = 1,
    kNalIdrSlice = 5,
    kNalSps = 7,
    kNalPps = 8,
    kNalStapA = 24,
    kNalFuA = 28,
};

// Which parameter set a NAL unit is, if any. H.264 has no VPS.
enum class ParameterSet { None, Vps, Sps, Pps };

class RtpPayloadFormat {
public:
    virtual ~RtpPayloadFormat() = default;

    // The built-in format. Stateless and shared: one instance for the program.
    static const RtpPayloadFormat& H264();

    virtual VideoCodec Codec() const = 0;
    // The SDP encoding name, as in "a=rtpmap:96 H264/90000".
    virtual const char* EncodingName() const = 0;
    virtual uint8_t NalType(const std::string& nal) const = 0;
    // A NAL unit a player can start decoding from: an H.264 IDR slice, an H.265 IRAP picture.
    virtual bool IsKeyFrame(const std::string& nal) const = 0;
    virtual ParameterSet ParameterSetOf(const std::string& nal) const = 0;
    // The parameter sets a decoder needs, in the order they go before a key frame.
    virtual std::vector<ParameterSet> RequiredParameterSets() const = 0;
    // The SDP fmtp parameters for these parameter sets (indexed by ParameterSet), so
    // players can set up the decoder before the first key frame. Empty if there's nothing
    // to say yet.
    virtual std::string Fmtp(const std::string sets[4]) const = 0;

    // Packetizes one access unit (a frame's NAL units) with the marker bit on its last
    // packet. Packets are at most maxPacketSize bytes. Advances `seq` by the packet count.
    void Packetize(const std::vector<std::string>& nalUnits, uint32_t timestamp, uint32_t ssrc,
                   size_t maxPacketSize, uint16_t& seq, std::vector<std::string>& packets) const;

    // Receiver side: appends the NAL units in one RTP payload to `annexB`, with start codes.
    // `inFragment` carries a half-received fragment between calls. False if the payload is
    // malformed or continues a fragment whose start was lost.
    virtual bool Depacketize(const uint8_t* payload, size_t size, bool& inFragment,
                             std::string& annexB) const = 0;

    // The NAL header size (1 or 2 bytes), and the fragmentation unit's headers for `nal`:
    // the payload header that replaces the NAL header, and the fragment header's type bits.
    virtual size_t NalHeaderSize() const = 0;
    virtual std::string FragmentPayloadHeader(const std::string& nal) const = 0;

protected:
    // What the formats share, for theirs and the plugins' codecs.
    static std::string Base64(const std::string& data);
    // A whole NAL unit in one payload: appended with its start code.
    static void AppendNal(const uint8_t* payload, size_t size, std::string& annexB);
    // Aggregation packets (H.264 STAP-A, H.265 AP): after the `headerSize`-byte payload
    // header, NAL units each with a 16-bit size. False if one runs past the end.
    static bool UnpackAggregate(const uint8_t* payload, size_t size, size_t headerSize,
                                std::string& annexB);
    // Fragmentation units: the start fragment rebuilds the NAL header from `header` (its
    // `headerSize` bytes), every fragment adds its data after `dataAt`. False for a fragment
    // whose start was lost.
    static bool AddFragment(const uint8_t* payload, size_t size, size_t dataAt,
                            uint8_t fragmentHeader, const char* header, size_t headerSize,
                            bool& inFragment, std::string& annexB);
};

// Splits an Annex B byte stream (00 00 01 / 00 00 00 01 start codes) into NAL units,
// without their start codes. The same for every codec.
void SplitAnnexB(const std::string& annexB, std::vector<std::string>& nalUnits);

// Receiver side of RtpPayloadFormat: rebuilds NAL units from RTP payloads.
class RtpVideoDepacketizer {
public:
    explicit RtpVideoDepacketizer(const RtpPayloadFormat& format = RtpPayloadFormat::H264())
        : m_format(&format) {}

    // False if the payload is malformed or continues a fragment whose start was lost; the
    // caller should then treat the frame as damaged.
    bool Add(const uint8_t* payload, size_t size, std::string& annexB) {
        return m_format->Depacketize(payload, size, m_inFragment, annexB);
    }
    // Forget a half-received fragment, after a lost packet.
    void Reset() { m_inFragment = false; }

private:
    const RtpPayloadFormat* m_format;
    bool m_inFragment = false;
};

// The latest parameter sets of a stream, as they come out of the encoder. Key frames get
// them put in front, for players that join on that frame, and the SDP carries them.
class ParameterSetCache {
public:
    explicit ParameterSetCache(const RtpPayloadFormat& format) : m_format(&format) {}

    // Takes note of the parameter sets among `nalUnits`. True if any changed.
    bool Update(const std::vector<std::string>& nalUnits);
    // Puts the parameter sets in front of a key frame that doesn't carry them itself.
    void PrependTo(std::vector<std::string>& nalUnits) const;
    std::string Fmtp() const { return m_format->Fmtp(m_sets); }
    void Clear();

private:
    bool HasAll() const;

    const RtpPayloadFormat* m_format;
    std::string m_sets[4];  // indexed by ParameterSet
};

}  // namespace gazeora
