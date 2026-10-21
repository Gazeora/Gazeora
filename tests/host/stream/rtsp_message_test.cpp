#include <gtest/gtest.h>

#include <string>

#include "gazeora/host/stream/rtsp_message.h"

namespace gazeora {
namespace {

TEST(RtspRequestTest, ParsesMethodUrlAndHeadersCaseInsensitively) {
    RtspRequest request;
    ASSERT_TRUE(RtspRequest::Parse(
        "SETUP rtsp://pc:8554/track1 RTSP/1.0\r\nCSeq: 3\r\nsession:  ABC;timeout=60 \r\n\r\n",
        request));
    EXPECT_EQ(request.method, "SETUP");
    EXPECT_EQ(request.url, "rtsp://pc:8554/track1");
    EXPECT_EQ(request.Header("cseq"), "3");
    EXPECT_EQ(request.SessionId(), "ABC");
    EXPECT_EQ(request.Header("Missing"), "");
}

TEST(RtspRequestTest, RejectsWhatIsNotRtsp) {
    RtspRequest request;
    EXPECT_FALSE(RtspRequest::Parse("garbage\r\n\r\n", request));
}

TEST(RtspResponseTest, EchoesCSeqAndCountsTheBody) {
    RtspRequest request;
    ASSERT_TRUE(RtspRequest::Parse("DESCRIBE / RTSP/1.0\r\nCSeq: 7\r\n\r\n", request));
    std::string text =
        RtspResponse("200 OK", request).Header("Content-Base", "/").Body("application/sdp", "v=0")
            .Serialize();
    EXPECT_EQ(text,
              "RTSP/1.0 200 OK\r\nCSeq: 7\r\nServer: Gazeora\r\nContent-Base: /\r\n"
              "Content-Type: application/sdp\r\nContent-Length: 3\r\n\r\nv=0");
}

TEST(TransportSpecTest, ParsesUdpAndInterleaved) {
    TransportSpec udp;
    ASSERT_TRUE(TransportSpec::Parse("RTP/AVP;unicast;client_port=5000-5001", udp));
    EXPECT_FALSE(udp.interleaved);
    EXPECT_EQ(udp.rtpPort, 5000);
    EXPECT_EQ(udp.rtcpPort, 5001);

    TransportSpec oneport;
    ASSERT_TRUE(TransportSpec::Parse("RTP/AVP;unicast;client_port=6000", oneport));
    EXPECT_EQ(oneport.rtcpPort, 6001);

    TransportSpec tcp;
    ASSERT_TRUE(TransportSpec::Parse("RTP/AVP/TCP;unicast;interleaved=2-3", tcp));
    EXPECT_TRUE(tcp.interleaved);
    EXPECT_EQ(tcp.channel, 2);
}

TEST(TransportSpecTest, RefusesWhatTheServerDoesNotOffer) {
    TransportSpec spec;
    EXPECT_FALSE(TransportSpec::Parse("RTP/AVP;multicast", spec));
    EXPECT_FALSE(TransportSpec::Parse("RTP/AVP/TCP;unicast;client_port=5000-5001", spec));
    EXPECT_FALSE(TransportSpec::Parse("RTP/AVP;unicast", spec));
    EXPECT_FALSE(TransportSpec::Parse("RTP/AVP/TCP;interleaved=255", spec));
    EXPECT_FALSE(TransportSpec::Parse("RTP/AVP;client_port=65535", spec));
}

TEST(RtspReaderTest, SplitsRequestsAndSkipsInterleavedFrames) {
    RtspReader reader;
    std::string out;
    std::string first = "OPTIONS * RTSP/1.0\r\nCSeq: 1\r\n\r\n";
    std::string second = "SET_PARAMETER * RTSP/1.0\r\nCSeq: 2\r\nContent-Length: 4\r\n\r\nab";
    reader.Append(first.data(), first.size());
    reader.Append("$\x01\x00\x02xy", 6);
    reader.Append(second.data(), second.size());

    ASSERT_EQ(reader.Next(out), RtspReader::Item::Request);
    EXPECT_EQ(out, first);
    EXPECT_EQ(reader.Next(out), RtspReader::Item::Interleaved);
    EXPECT_EQ(reader.Next(out), RtspReader::Item::None) << "the body isn't complete yet";
    reader.Append("cd", 2);
    ASSERT_EQ(reader.Next(out), RtspReader::Item::Request);
    EXPECT_EQ(out, second + "cd");
    EXPECT_EQ(reader.Next(out), RtspReader::Item::None);
}

TEST(RtspReaderTest, FlagsOversizedRequests) {
    RtspReader reader;
    std::string out;
    std::string flood(20000, 'a');
    reader.Append(flood.data(), flood.size());
    EXPECT_EQ(reader.Next(out), RtspReader::Item::TooLarge);
}

}  // namespace
}  // namespace gazeora
