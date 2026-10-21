#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gazeora/host/stream/rtp_video.h"

namespace gazeora {
namespace {

std::string Nal(uint8_t header, size_t size, char fill = 'x') {
    return std::string(1, char(header)) + std::string(size - 1, fill);
}

// An H.265 NAL unit: the two-byte header (type in bits 1-6, layer 0, temporal id 1).
std::string HevcNal(uint8_t type, size_t size, char fill = 'x') {
    return std::string(1, char(type << 1)) + '\x01' + std::string(size - 2, fill);
}

TEST(RtpH264Test, SplitsAnnexBWithBothStartCodeLengths) {
    std::string stream = std::string("\0\0\0\1", 4) +
                         "\x67"
                         "ab" +
                         std::string("\0\0\1", 3) +
                         "\x68"
                         "c" +
                         std::string("\0\0\0\1", 4) +
                         "\x65"
                         "def";
    std::vector<std::string> nals;
    SplitAnnexB(stream, nals);
    ASSERT_EQ(nals.size(), 3u);
    EXPECT_EQ(nals[0],
              "\x67"
              "ab");
    EXPECT_EQ(nals[1],
              "\x68"
              "c");
    EXPECT_EQ(nals[2],
              "\x65"
              "def");
    EXPECT_EQ(NalType(nals[0]), kNalSps);
    EXPECT_EQ(NalType(nals[2]), kNalIdrSlice);
}

TEST(RtpH264Test, SmallNalUnitsGoAloneAndLargeOnesAsFragments) {
    std::vector<std::string> nals = {Nal(0x67, 20), Nal(0x68, 5), Nal(0x65, 3000, 'y')};
    uint16_t seq = 65530;
    std::vector<std::string> packets;
    PacketizeRtpVideo(VideoCodec::H264, nals, 0x11223344, 0xCAFEF00D, 1400, seq, packets);
    // 2 single NAL packets, then 3000 bytes in fragments of up to 1386.
    ASSERT_EQ(packets.size(), 5u);
    EXPECT_EQ(seq, uint16_t(65530 + 5));
    for (size_t i = 0; i < packets.size(); ++i) {
        const auto* p = reinterpret_cast<const uint8_t*>(packets[i].data());
        EXPECT_LE(packets[i].size(), 1400u);
        EXPECT_EQ(p[0], 0x80);
        EXPECT_EQ(p[1] & 0x7F, kRtpVideoPayloadType);
        EXPECT_EQ((p[1] & 0x80) != 0, i + 1 == packets.size()) << "marker on the last only";
    }
    auto payload = [&](size_t i) {
        return reinterpret_cast<const uint8_t*>(packets[i].data()) + 12;
    };
    EXPECT_EQ(payload(0)[0], 0x67);
    EXPECT_EQ(payload(2)[0], 0x60 | kNalFuA);  // the IDR's NRI, FU-A
    EXPECT_EQ(payload(2)[1], 0x80 | 5);        // start
    EXPECT_EQ(payload(3)[1], 5);               // middle
    EXPECT_EQ(payload(4)[1], 0x40 | 5);        // end

    RtpVideoDepacketizer depacketizer;
    std::string annexB;
    for (const std::string& packet : packets) {
        ASSERT_TRUE(depacketizer.Add(reinterpret_cast<const uint8_t*>(packet.data()) + 12,
                                     packet.size() - 12, annexB));
    }
    std::vector<std::string> rebuilt;
    SplitAnnexB(annexB, rebuilt);
    EXPECT_EQ(rebuilt, nals);
}

TEST(RtpH264Test, AFragmentWithoutItsStartIsRejected) {
    std::vector<std::string> packets;
    uint16_t seq = 0;
    PacketizeRtpVideo(VideoCodec::H264, {Nal(0x65, 4000)}, 0, 0, 1400, seq, packets);
    ASSERT_EQ(packets.size(), 3u);
    RtpVideoDepacketizer depacketizer;
    std::string annexB;
    // The first fragment was lost.
    EXPECT_FALSE(depacketizer.Add(reinterpret_cast<const uint8_t*>(packets[1].data()) + 12,
                                  packets[1].size() - 12, annexB));
    EXPECT_TRUE(annexB.empty());
}

TEST(RtpH264Test, FmtpCarriesProfileAndParameterSets) {
    std::string sps = "\x67\x42\xC0\x28";  // Constrained Baseline, level 4.0
    std::string pps = "\x68\xCE";
    EXPECT_EQ(H264Fmtp(sps, pps),
              "packetization-mode=1;profile-level-id=42C028;sprop-parameter-sets=Z0LAKA==,aM4=");
    EXPECT_EQ(H264Fmtp("", ""), "packetization-mode=1");
}

// A decoder for `codec`: Microsoft's for H.264, which ships with Windows; for H.265 the
// first registered one (the HEVC Video Extensions), if any. Null if there's none.

TEST(RtpH265Test, SmallNalUnitsGoAloneAndLargeOnesAsFragments) {
    std::vector<std::string> nals = {HevcNal(kHevcVps, 24), HevcNal(kHevcSps, 40),
                                     HevcNal(kHevcPps, 8), HevcNal(kHevcIdrWRadl, 3000, 'y')};
    uint16_t seq = 7;
    std::vector<std::string> packets;
    PacketizeRtpVideo(VideoCodec::H265, nals, 1234, 0xCAFEF00D, 1400, seq, packets);
    // 3 single NAL packets, then 2998 bytes after the header in fragments of up to 1385.
    ASSERT_EQ(packets.size(), 6u);
    EXPECT_EQ(seq, 13);
    auto payload = [&](size_t i) {
        return reinterpret_cast<const uint8_t*>(packets[i].data()) + 12;
    };
    EXPECT_EQ(payload(0)[0] >> 1, kHevcVps);
    EXPECT_EQ(payload(3)[0] >> 1 & 0x3F, kHevcFu);  // payload header: type 49,
    EXPECT_EQ(payload(3)[1], 0x01);                 // the original layer and temporal id
    EXPECT_EQ(payload(3)[2], 0x80 | kHevcIdrWRadl); // start
    EXPECT_EQ(payload(4)[2], kHevcIdrWRadl);        // middle
    EXPECT_EQ(payload(5)[2], 0x40 | kHevcIdrWRadl); // end
    for (size_t i = 0; i < packets.size(); ++i) {
        EXPECT_LE(packets[i].size(), 1400u);
        EXPECT_EQ((uint8_t(packets[i][1]) & 0x80) != 0, i + 1 == packets.size());
    }

    RtpVideoDepacketizer depacketizer(VideoCodec::H265);
    std::string annexB;
    for (const std::string& packet : packets) {
        ASSERT_TRUE(depacketizer.Add(reinterpret_cast<const uint8_t*>(packet.data()) + 12,
                                     packet.size() - 12, annexB));
    }
    std::vector<std::string> rebuilt;
    SplitAnnexB(annexB, rebuilt);
    EXPECT_EQ(rebuilt, nals);
    EXPECT_TRUE(IsKeyFrameNal(VideoCodec::H265, rebuilt[3]));
    EXPECT_FALSE(IsKeyFrameNal(VideoCodec::H265, HevcNal(kHevcTrailR, 10)));
}

TEST(RtpH265Test, UnpacksAggregationPackets) {
    // Payload header type 48, then two NAL units, each with a 16-bit size.
    std::string vps = HevcNal(kHevcVps, 5), sps = HevcNal(kHevcSps, 7);
    std::string ap = std::string(1, char(kHevcAp << 1)) + '\x01';
    for (const std::string* nal : {&vps, &sps}) {
        ap += char(nal->size() >> 8);
        ap += char(nal->size());
        ap += *nal;
    }
    RtpVideoDepacketizer depacketizer(VideoCodec::H265);
    std::string annexB;
    ASSERT_TRUE(depacketizer.Add(reinterpret_cast<const uint8_t*>(ap.data()), ap.size(), annexB));
    std::vector<std::string> nals;
    SplitAnnexB(annexB, nals);
    EXPECT_EQ(nals, (std::vector<std::string>{vps, sps}));
}

TEST(RtpH265Test, AFragmentWithoutItsStartIsRejected) {
    std::vector<std::string> packets;
    uint16_t seq = 0;
    PacketizeRtpVideo(VideoCodec::H265, {HevcNal(kHevcTrailR, 4000)}, 0, 0, 1400, seq, packets);
    ASSERT_EQ(packets.size(), 3u);
    RtpVideoDepacketizer depacketizer(VideoCodec::H265);
    std::string annexB;
    EXPECT_FALSE(depacketizer.Add(reinterpret_cast<const uint8_t*>(packets[1].data()) + 12,
                                  packets[1].size() - 12, annexB));
    EXPECT_TRUE(annexB.empty());
}

TEST(RtpH265Test, FmtpCarriesTheThreeParameterSets) {
    EXPECT_EQ(H265Fmtp("\x40\x01", "\x42\x01", "\x44\x01"),
              "sprop-vps=QAE=;sprop-sps=QgE=;sprop-pps=RAE=");
    EXPECT_EQ(H265Fmtp("", "\x42\x01", "\x44\x01"), "");
}

}  // namespace
}  // namespace gazeora
