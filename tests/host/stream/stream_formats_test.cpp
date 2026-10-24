#include "gazeora/host/stream/discovery.h"
#include "gazeora/host/stream/frame_packet.h"
#include "gazeora/host/stream/stream_protocol.h"
#include "gazeora/host/stream/ts_muxer.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace gazeora {
namespace {

TEST(StreamProtocolTest, IdsRoundTrip) {
    for (StreamProtocol protocol : kAllStreamProtocols) {
        StreamProtocol parsed = StreamProtocol::Usb;
        ASSERT_TRUE(ParseProtocolId(ProtocolId(protocol), parsed)) << ProtocolId(protocol);
        EXPECT_EQ(parsed, protocol);
    }
    StreamProtocol unchanged = StreamProtocol::Http;
    EXPECT_FALSE(ParseProtocolId("smoke-signals", unchanged));
    EXPECT_EQ(unchanged, StreamProtocol::Http);
}

TEST(FramePacketTest, HeaderRoundTripsBigEndian) {
    FrameHeader header;
    header.type = FrameMessage::Hello;
    header.flags = kFrameFlagKey | kFrameFlagH265;
    header.width = 1920;
    header.height = 1080;
    header.lossPerMille = 25;
    header.frameNumber = 0x01020304;
    header.timestamp = 0xA0B0C0D0;
    header.tail = 0x00030007;
    uint8_t raw[kFrameHeaderSize];
    header.Write(raw);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(raw), 4), "GZF1");
    EXPECT_EQ(raw[6], 0x07);  // 1920, high byte first
    EXPECT_EQ(raw[7], 0x80);
    EXPECT_EQ(raw[12], 0x01);

    FrameHeader read;
    ASSERT_TRUE(read.Read(raw, sizeof(raw)));
    EXPECT_EQ(read.type, FrameMessage::Hello);
    EXPECT_EQ(read.flags, header.flags);
    EXPECT_EQ(read.width, 1920);
    EXPECT_EQ(read.height, 1080);
    EXPECT_EQ(read.lossPerMille, 25);
    EXPECT_EQ(read.frameNumber, 0x01020304u);
    EXPECT_EQ(read.timestamp, 0xA0B0C0D0u);
    EXPECT_EQ(read.tail, 0x00030007u);

    EXPECT_FALSE(read.Read(raw, kFrameHeaderSize - 1));
    raw[0] = 'X';
    EXPECT_FALSE(read.Read(raw, sizeof(raw)));
}

TEST(FramePacketTest, ChunksAFrameIntoDatagramsThatFit) {
    std::string annexB(5000, 'a');
    for (size_t i = 0; i < annexB.size(); ++i) annexB[i] = char(i);
    FrameHeader header;
    header.frameNumber = 9;
    std::vector<std::string> datagrams;
    ChunkFrame(header, annexB, datagrams);
    ASSERT_EQ(datagrams.size(), 4u);  // 1376 bytes of payload each
    std::string joined;
    for (size_t i = 0; i < datagrams.size(); ++i) {
        EXPECT_LE(datagrams[i].size(), kMaxFrameDatagram);
        FrameHeader read;
        ASSERT_TRUE(
            read.Read(reinterpret_cast<const uint8_t*>(datagrams[i].data()), datagrams[i].size()));
        EXPECT_EQ(read.frameNumber, 9u);
        EXPECT_EQ(read.tail, uint32_t(i) << 16 | 4u);
        joined += datagrams[i].substr(kFrameHeaderSize);
    }
    EXPECT_EQ(joined, annexB);

    ChunkFrame(header, "", datagrams);
    EXPECT_EQ(datagrams.size(), 1u) << "an empty frame still says it happened";
}

TEST(FramePacketTest, StreamMessageCarriesItsLength) {
    std::string message = StreamMessage(FrameHeader(), "payload");
    ASSERT_EQ(message.size(), kFrameHeaderSize + 7);
    FrameHeader read;
    ASSERT_TRUE(read.Read(reinterpret_cast<const uint8_t*>(message.data()), message.size()));
    EXPECT_EQ(read.tail, 7u);
    EXPECT_EQ(message.substr(kFrameHeaderSize), "payload");
}

TEST(TsCrcTest, IsMpeg2) {
    const std::string check = "123456789";
    EXPECT_EQ(Crc32Mpeg2(reinterpret_cast<const uint8_t*>(check.data()), check.size()),
              0x0376E6E7u);
}

class TsMuxerTest : public ::testing::Test {
protected:
    static uint16_t Pid(const std::string& ts, size_t packet) {
        size_t at = packet * TsMuxer::kPacketSize;
        return uint16_t((uint8_t(ts[at + 1]) & 0x1F) << 8 | uint8_t(ts[at + 2]));
    }
    static size_t PayloadStart(const std::string& ts, size_t packet) {
        size_t at = packet * TsMuxer::kPacketSize;
        bool field = uint8_t(ts[at + 3]) & 0x20;
        return at + 4 + (field ? 1 + uint8_t(ts[at + 4]) : 0);
    }
    // The PES payload (everything after its header) of one frame starting at `packet`.
    static std::string PesPayload(const std::string& ts, size_t packet) {
        std::string pes;
        for (size_t p = packet; p * TsMuxer::kPacketSize < ts.size(); ++p) {
            if (p > packet && (uint8_t(ts[p * TsMuxer::kPacketSize + 1]) & 0x40)) break;
            size_t start = PayloadStart(ts, p);
            pes.append(ts, start, (p + 1) * TsMuxer::kPacketSize - start);
        }
        return pes.substr(9 + uint8_t(pes[8]));
    }

    const std::string keyFrame = std::string("\0\0\0\1\x67\x42\xC0\x28\0\0\0\1\x68\xCE", 14) +
                                 std::string("\0\0\0\1\x65", 5) + std::string(1000, 'i');
};

TEST_F(TsMuxerTest, KeyFramesComeAfterTheTablesAndCarryTheClock) {
    TsMuxer muxer;
    std::string ts;
    muxer.Mux(VideoCodec::H264, keyFrame, 90000, true, ts);
    ASSERT_EQ(ts.size() % TsMuxer::kPacketSize, 0u);
    ASSERT_GE(ts.size() / TsMuxer::kPacketSize, 8u);  // PAT, PMT, 6+ of video
    for (size_t at = 0; at < ts.size(); at += TsMuxer::kPacketSize) {
        EXPECT_EQ(uint8_t(ts[at]), 0x47);
    }
    EXPECT_EQ(Pid(ts, 0), 0);
    EXPECT_EQ(Pid(ts, 1), TsMuxer::kPmtPid);
    for (size_t p = 2; p < ts.size() / TsMuxer::kPacketSize; ++p) {
        EXPECT_EQ(Pid(ts, p), TsMuxer::kVideoPid);
    }

    // The PAT and PMT end in a valid CRC: running it over the whole section gives 0.
    for (size_t p = 0; p < 2; ++p) {
        const uint8_t* section =
            reinterpret_cast<const uint8_t*>(ts.data()) + p * TsMuxer::kPacketSize + 5;
        size_t length = 3 + ((section[1] & 0x0F) << 8 | section[2]);
        EXPECT_EQ(Crc32Mpeg2(section, length), 0u) << "section " << p;
    }
    // The PMT names H.264.
    EXPECT_EQ(uint8_t(ts[TsMuxer::kPacketSize + 5 + 12]), 0x1B);

    // The first video packet: random access, the PCR at the frame's time, then the PES
    // header with a PTS 100 ms later.
    const size_t video = 2 * TsMuxer::kPacketSize;
    EXPECT_EQ(uint8_t(ts[video + 1]) & 0x40, 0x40) << "payload unit start";
    EXPECT_EQ(uint8_t(ts[video + 5]), 0x50) << "random access and PCR";
    uint64_t pcr = uint64_t(uint8_t(ts[video + 6])) << 25 | uint64_t(uint8_t(ts[video + 7])) << 17 |
                   uint64_t(uint8_t(ts[video + 8])) << 9 | uint64_t(uint8_t(ts[video + 9])) << 1 |
                   uint8_t(ts[video + 10]) >> 7;
    EXPECT_EQ(pcr, 90000u);
    size_t pes = PayloadStart(ts, 2);
    EXPECT_EQ(ts.substr(pes, 4), std::string("\0\0\1\xE0", 4));
    const uint8_t* p = reinterpret_cast<const uint8_t*>(ts.data()) + pes + 9;
    uint64_t pts = uint64_t(p[0] & 0x0E) << 29 | uint64_t(p[1]) << 22 | uint64_t(p[2] >> 1) << 15 |
                   uint64_t(p[3]) << 7 | p[4] >> 1;
    EXPECT_EQ(pts, 99000u);

    // The access unit, after the delimiter TS needs.
    std::string payload = PesPayload(ts, 2);
    EXPECT_EQ(payload, std::string("\0\0\0\1\x09\xF0", 6) + keyFrame);
}

TEST_F(TsMuxerTest, OtherFramesSkipTheTablesAndCountOn) {
    TsMuxer muxer;
    std::string first, second;
    muxer.Mux(VideoCodec::H264, keyFrame, 0, true, first);
    muxer.Mux(VideoCodec::H264, std::string("\0\0\0\1\x41pp", 7), 3000, false, second);
    ASSERT_EQ(second.size(), TsMuxer::kPacketSize) << "a small frame fits one packet";
    EXPECT_EQ(Pid(second, 0), TsMuxer::kVideoPid);
    EXPECT_EQ(uint8_t(second[5]) & 0x40, 0) << "not a random access point";
    // Continuity runs on from the last video packet of the first frame.
    uint8_t last = uint8_t(first[first.size() - TsMuxer::kPacketSize + 3]) & 0x0F;
    EXPECT_EQ(uint8_t(second[3]) & 0x0F, (last + 1) & 0x0F);
    EXPECT_EQ(PesPayload(second, 0),
              std::string("\0\0\0\1\x09\xF0", 6) + std::string("\0\0\0\1\x41pp", 7));
}

TEST_F(TsMuxerTest, H265GetsItsStreamTypeAndDelimiter) {
    TsMuxer muxer;
    std::string ts;
    const std::string irap = std::string("\0\0\0\1\x26\x01", 6) + std::string(300, 'k');
    muxer.Mux(VideoCodec::H265, irap, 0, true, ts);
    EXPECT_EQ(uint8_t(ts[TsMuxer::kPacketSize + 5 + 12]), 0x24);
    EXPECT_EQ(PesPayload(ts, 2), std::string("\0\0\0\1\x46\x01\x50", 7) + irap);
}

TEST_F(TsMuxerTest, KeepsADelimiterTheEncoderAlreadyPut) {
    TsMuxer muxer;
    std::string ts;
    const std::string withDelimiter = std::string("\0\0\0\1\x09\x10", 6) + keyFrame;
    muxer.Mux(VideoCodec::H264, withDelimiter, 0, true, ts);
    EXPECT_EQ(PesPayload(ts, 2), withDelimiter);
}

TEST(DiscoveryTest, JsonStringsAreEscaped) {
    EXPECT_EQ(JsonString("My PC"), "\"My PC\"");
    EXPECT_EQ(JsonString("a\"b\\c\n"), "\"a\\\"b\\\\c\\u000a\"");
}

}  // namespace
}  // namespace gazeora
