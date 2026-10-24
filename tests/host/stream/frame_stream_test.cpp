#include "gazeora/host/stream/frame_stream.h"
#include "gazeora/common/text/utf8.h"
#include "gazeora/host/stream/frame_packet.h"
#include "gazeora/host/stream/rtp_video.h"
#include "gazeora/host/stream/ts_muxer.h"
#include "host/stream/software_only_encoder.h"

#include <gtest/gtest.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace gazeora {
namespace {

// Talks to the server like an RTSP player: requests over TCP, RTP over UDP or interleaved
// in the RTSP connection. Loopback
// only, so the tests don't need a firewall exception.
class FrameStreamTest : public ::testing::Test {
protected:
    void SetUp() override {
        WSADATA wsa;
        ASSERT_EQ(WSAStartup(MAKEWORD(2, 2), &wsa), 0);
        StartServer(StreamProtocol::RtspUdp);
    }

    // (Re)starts the server with `protocol`, as the app does when it's changed.
    void StartServer(StreamProtocol protocol) {
        server.Stop();
        server.SetProtocol(protocol);
        server.SetDiscoveryPort(0);  // any free port, so tests running at once don't collide
        std::wstring error;
        ASSERT_TRUE(server.Start(0, false, error)) << Utf8::FromWide(error);
        ASSERT_NE(server.Port(), 0);
        base = "rtsp://127.0.0.1:" + std::to_string(server.Port()) + "/";
    }

    // Waits up to a second for the encode thread to see the viewers.
    void WaitForViewers(int viewers) {
        for (int i = 0; i < 100 && server.Viewers() != viewers; ++i) Sleep(10);
    }

    void TearDown() override {
        if (rtsp != INVALID_SOCKET) closesocket(rtsp);
        if (rtp != INVALID_SOCKET) closesocket(rtp);
        if (rtcpSink != INVALID_SOCKET) closesocket(rtcpSink);
        server.Stop();
        WSACleanup();
    }

    static sockaddr_in Loopback(uint16_t port) {
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        return address;
    }

    static void SetTimeout(SOCKET s, DWORD ms) {
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&ms), sizeof(ms));
    }

    void Connect() {
        rtsp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        SetTimeout(rtsp, 5000);
        sockaddr_in address = Loopback(server.Port());
        ASSERT_EQ(connect(rtsp, reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);
    }

    // Sends one request and reads its response, body included.
    std::string Request(const std::string& method, const std::string& url,
                        const std::string& headers = {}) {
        std::string request = method + " " + url + " RTSP/1.0\r\nCSeq: " + std::to_string(++cseq) +
                              "\r\n" + headers + "\r\n";
        send(rtsp, request.data(), int(request.size()), 0);
        // Interleaved RTP may come before or after the response; it stays in `received`.
        while (true) {
            size_t at = received.find("RTSP/1.0 ");
            size_t headEnd = at == std::string::npos ? at : received.find("\r\n\r\n", at);
            if (headEnd != std::string::npos) {
                size_t lengthAt = received.find("Content-Length: ", at);
                size_t length =
                    lengthAt < headEnd ? strtoul(&received[lengthAt + 16], nullptr, 10) : 0;
                if (received.size() >= headEnd + 4 + length) {
                    std::string response = received.substr(at, headEnd + 4 + length - at);
                    received.erase(at, response.size());
                    return response;
                }
            }
            if (!ReceiveMore()) return received;
        }
    }

    bool ReceiveMore() {
        char buffer[65536];
        int got = recv(rtsp, buffer, sizeof(buffer), 0);
        if (got <= 0) return false;
        received.append(buffer, size_t(got));
        return true;
    }

    // The next interleaved packet on `channel`, skipping other channels; empty on timeout.
    std::string ReceiveInterleaved(uint8_t channel) {
        while (true) {
            if (received.size() >= 4) {
                EXPECT_EQ(received[0], '$');
                size_t size = uint8_t(received[2]) << 8 | uint8_t(received[3]);
                if (received.size() >= 4 + size) {
                    std::string packet = received.substr(4, size);
                    bool match = uint8_t(received[1]) == channel;
                    received.erase(0, 4 + size);
                    if (match) return packet;
                    continue;
                }
            }
            if (!ReceiveMore()) return {};
        }
    }

    static std::string Header(const std::string& response, const std::string& name) {
        size_t at = response.find("\r\n" + name + ": ");
        if (at == std::string::npos) return {};
        at += name.size() + 4;
        return response.substr(at, response.find("\r\n", at) - at);
    }

    // DESCRIBE, SETUP over UDP and PLAY; returns the session id.
    std::string Play() {
        Connect();
        // Bind both ports of the pair, like a real player. The server sends its sender reports
        // to the RTCP port; left unbound, it can be another test process's RTP port (ephemeral
        // ports are handed out in sequence), which then receives this server's reports.
        uint16_t port = 0;
        if (rtcpSink != INVALID_SOCKET) closesocket(rtcpSink);
        rtcpSink = INVALID_SOCKET;
        for (int attempt = 0; attempt < 50 && rtcpSink == INVALID_SOCKET; ++attempt) {
            if (rtp != INVALID_SOCKET) closesocket(rtp);
            rtp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in address = Loopback(0);
            int length = sizeof(address);
            if (bind(rtp, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) continue;
            getsockname(rtp, reinterpret_cast<sockaddr*>(&address), &length);
            port = ntohs(address.sin_port);
            if (port == 65535) continue;
            SOCKET sink = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in next = Loopback(uint16_t(port + 1));
            if (bind(sink, reinterpret_cast<sockaddr*>(&next), sizeof(next)) == 0) {
                rtcpSink = sink;
            } else {
                closesocket(sink);
            }
        }
        EXPECT_NE(rtcpSink, INVALID_SOCKET) << "no free pair of adjacent UDP ports";
        if (rtcpSink == INVALID_SOCKET) return {};
        SetTimeout(rtp, 3000);
        int buffer = 8 << 20;
        setsockopt(rtp, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&buffer), sizeof(buffer));

        Request("DESCRIBE", base);
        std::string setup =
            Request("SETUP", base + "track1",
                    "Transport: RTP/AVP;unicast;client_port=" + std::to_string(port) + "-" +
                        std::to_string(port + 1) + "\r\n");
        EXPECT_EQ(setup.rfind("RTSP/1.0 200 OK\r\n", 0), 0u) << setup;
        std::string transport = Header(setup, "Transport");
        size_t ssrcAt = transport.find("ssrc=");
        EXPECT_NE(ssrcAt, std::string::npos) << transport;
        ssrc = uint32_t(strtoul(transport.c_str() + ssrcAt + 5, nullptr, 16));
        size_t portsAt = transport.find("server_port=");
        EXPECT_NE(portsAt, std::string::npos) << transport;
        serverRtcpPort =
            uint16_t(strtoul(transport.c_str() + transport.find('-', portsAt) + 1, nullptr, 10));
        std::string session = Header(setup, "Session");
        session = session.substr(0, session.find(';'));
        std::string play = Request("PLAY", base, "Session: " + session + "\r\n");
        EXPECT_EQ(play.rfind("RTSP/1.0 200 OK\r\n", 0), 0u) << play;
        return session;
    }

    // Receives the next whole frame (up to its marker bit). Returns its packet count, or 0
    // on timeout; `nalTypes` gets the types of the NAL units in it.
    int ReceiveFrame(std::vector<uint8_t>* nalTypes = nullptr) {
        char packet[2048];
        RtpVideoDepacketizer depacketizer;
        std::string annexB;
        int packets = 0;
        for (int i = 0; i < 10000; ++i) {
            int got = recv(rtp, packet, sizeof(packet), 0);
            if (got <= 0) return 0;
            const auto* p = reinterpret_cast<const uint8_t*>(packet);
            EXPECT_EQ(p[1] & 0x7F, kRtpVideoPayloadType);
            EXPECT_EQ(uint32_t(p[8]) << 24 | p[9] << 16 | p[10] << 8 | p[11], ssrc);
            EXPECT_TRUE(depacketizer.Add(p + 12, size_t(got) - 12, annexB));
            ++packets;
            if (p[1] & 0x80) break;
        }
        if (nalTypes) {
            std::vector<std::string> nals;
            SplitAnnexB(annexB, nals);
            nalTypes->clear();
            for (const std::string& nal : nals)
                nalTypes->push_back(RtpPayloadFormat::For(VideoCodec::H264).NalType(nal));
        }
        return packets;
    }

    static bool Has(const std::vector<uint8_t>& types, uint8_t type) {
        return std::find(types.begin(), types.end(), type) != types.end();
    }

    static std::vector<uint8_t> Solid(uint32_t width, uint32_t height, uint8_t value) {
        std::vector<uint8_t> bgra(size_t(width) * height * 4, value);
        return bgra;
    }

    FrameStreamServer server{SoftwareOnlyEncoder::Create};
    std::string base;
    SOCKET rtsp = INVALID_SOCKET;
    SOCKET rtp = INVALID_SOCKET;
    SOCKET rtcpSink = INVALID_SOCKET;  // holds the RTCP port of the pair; never read
    uint32_t ssrc = 0;
    uint16_t serverRtcpPort = 0;
    int cseq = 0;
    std::string received;  // read from the RTSP connection but not yet taken
};

TEST_F(FrameStreamTest, LoopbackServerOffersLoopbackUrl) {
    std::vector<std::wstring> urls = server.Urls();
    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], L"rtsp://127.0.0.1:" + std::to_wstring(server.Port()) + L"/");
}

TEST_F(FrameStreamTest, AnswersOptionsWithMethodsAndCSeq) {
    Connect();
    std::string response = Request("OPTIONS", base);
    EXPECT_EQ(response.rfind("RTSP/1.0 200 OK\r\n", 0), 0u) << response;
    EXPECT_EQ(Header(response, "CSeq"), "1");
    EXPECT_NE(Header(response, "Public").find("PLAY"), std::string::npos);
}

TEST_F(FrameStreamTest, DescribesOneH264VideoTrack) {
    Connect();
    std::string response = Request("DESCRIBE", base);
    EXPECT_EQ(response.rfind("RTSP/1.0 200 OK\r\n", 0), 0u) << response;
    EXPECT_EQ(Header(response, "Content-Type"), "application/sdp");
    EXPECT_EQ(Header(response, "Content-Base"), base);
    EXPECT_NE(response.find("m=video 0 RTP/AVP 96\r\n"), std::string::npos);
    EXPECT_NE(response.find("a=rtpmap:96 H264/90000\r\n"), std::string::npos);
    EXPECT_NE(response.find("a=fmtp:96 packetization-mode=1"), std::string::npos);
    EXPECT_NE(response.find("a=control:track1\r\n"), std::string::npos);
}

// Players such as ExoPlayer need the parameter sets in the SDP; they can't wait for them
// in-band.
TEST_F(FrameStreamTest, DescribeCarriesParameterSetsBeforeAnyoneWatches) {
    server.SetIdleFrame(64, 48, Solid(64, 48, 0x40));
    Connect();
    std::string response = Request("DESCRIBE", base);
    EXPECT_NE(response.find(";sprop-parameter-sets="), std::string::npos) << response;
}

TEST_F(FrameStreamTest, TcpPlayerThatFallsBehindResumesOnAKeyFrame) {
    StartServer(StreamProtocol::RtspTcp);
    Connect();
    int smallBuffer = 16 * 1024;
    setsockopt(rtsp, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&smallBuffer),
               sizeof(smallBuffer));
    std::string setup =
        Request("SETUP", base + "track1", "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n");
    std::string session = Header(setup, "Session");
    session = session.substr(0, session.find(';'));
    ASSERT_EQ(Request("PLAY", base, "Session: " + session + "\r\n").rfind("RTSP/1.0 200", 0), 0u);

    // Noise at 30 fps, so every frame is as large as the bitrate allows. Made up front, as
    // making it per frame is too slow in a debug build.
    std::vector<std::vector<uint8_t>> noise(4, std::vector<uint8_t>(1280 * 720 * 4));
    uint32_t seed = 1;
    for (auto& frame : noise) {
        for (uint8_t& byte : frame) byte = uint8_t((seed = seed * 1664525u + 1013904223u) >> 24);
    }
    std::atomic<bool> pushing{true};
    std::thread pusher([&] {
        std::vector<uint8_t> frame;
        for (size_t n = 0; pushing; ++n) {
            frame = noise[n % noise.size()];
            server.PushFrame(1280, 720, frame);
            Sleep(33);
        }
    });
    Sleep(2500);  // not reading: the backlog fills and frames get skipped

    // Read for a while. Somewhere there's a jump in time where frames were skipped, and the
    // frame after it must be a key frame, or the player couldn't decode what follows.
    RtpVideoDepacketizer depacketizer;
    std::string annexB;
    uint32_t lastTimestamp = 0, frameTimestamp = 0;
    bool haveLast = false, sawSkip = false, resumedOnKey = false;
    std::string seen;  // for the failure message
    int frames = 0;
    for (int i = 0; i < 20000 && !sawSkip; ++i) {
        std::string packet = ReceiveInterleaved(0);
        ASSERT_GT(packet.size(), 12u);
        const auto* p = reinterpret_cast<const uint8_t*>(packet.data());
        frameTimestamp = uint32_t(p[4]) << 24 | p[5] << 16 | p[6] << 8 | p[7];
        depacketizer.Add(p + 12, packet.size() - 12, annexB);
        if (!(p[1] & 0x80)) continue;
        // Not between the first two: opening the encoder can take a second, during which
        // newer frames replace the waiting one.
        if (frames >= 2 && frameTimestamp - lastTimestamp > 90000 * 3 / 10) {  // over 300 ms
            sawSkip = true;
            std::vector<std::string> nals;
            SplitAnnexB(annexB, nals);
            for (const std::string& nal : nals)
                resumedOnKey |=
                    RtpPayloadFormat::For(VideoCodec::H264).NalType(nal) == kNalIdrSlice;
            seen = "after " + std::to_string(frames) + " frames, " +
                   std::to_string((frameTimestamp - lastTimestamp) / 90) + " ms skipped; NAL types";
            for (const std::string& nal : nals)
                seen += " " + std::to_string(RtpPayloadFormat::For(VideoCodec::H264).NalType(nal));
        }
        ++frames;
        haveLast = true;
        lastTimestamp = frameTimestamp;
        annexB.clear();
    }
    pushing = false;
    pusher.join();
    EXPECT_TRUE(sawSkip) << frames << " frames";
    EXPECT_TRUE(resumedOnKey) << seen;
}

TEST_F(FrameStreamTest, RefusesMulticast) {
    Connect();
    std::string response =
        Request("SETUP", base + "track1", "Transport: RTP/AVP;multicast;port=5000-5001\r\n");
    EXPECT_EQ(response.rfind("RTSP/1.0 461 Unsupported Transport\r\n", 0), 0u) << response;
}

TEST_F(FrameStreamTest, RtspOverUdpRefusesInterleavedPlayers) {
    Connect();
    std::string response =
        Request("SETUP", base + "track1", "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n");
    EXPECT_EQ(response.rfind("RTSP/1.0 461 Unsupported Transport\r\n", 0), 0u) << response;
}

TEST_F(FrameStreamTest, RtspOverTcpRefusesUdpPlayers) {
    StartServer(StreamProtocol::RtspTcp);
    Connect();
    std::string response =
        Request("SETUP", base + "track1", "Transport: RTP/AVP;unicast;client_port=5000-5001\r\n");
    EXPECT_EQ(response.rfind("RTSP/1.0 461 Unsupported Transport\r\n", 0), 0u) << response;
}

TEST_F(FrameStreamTest, StreamsInterleavedOverTcp) {
    StartServer(StreamProtocol::RtspTcp);
    Connect();
    std::string setup =
        Request("SETUP", base + "track1", "Transport: RTP/AVP/TCP;unicast;interleaved=2-3\r\n");
    ASSERT_EQ(setup.rfind("RTSP/1.0 200 OK\r\n", 0), 0u) << setup;
    EXPECT_EQ(Header(setup, "Transport").rfind("RTP/AVP/TCP;unicast;interleaved=2-3;ssrc=", 0), 0u)
        << setup;
    std::string session = Header(setup, "Session");
    session = session.substr(0, session.find(';'));
    std::string play = Request("PLAY", base, "Session: " + session + "\r\n");
    ASSERT_EQ(play.rfind("RTSP/1.0 200 OK\r\n", 0), 0u) << play;
    EXPECT_EQ(server.Viewers(), 1);

    std::vector<uint8_t> frame = Solid(320, 240, 0x80);
    server.PushFrame(320, 240, frame);
    RtpVideoDepacketizer depacketizer;
    std::string annexB;
    for (int i = 0; i < 1000; ++i) {
        std::string packet = ReceiveInterleaved(2);
        ASSERT_GT(packet.size(), 12u);
        const auto* p = reinterpret_cast<const uint8_t*>(packet.data());
        EXPECT_EQ(p[1] & 0x7F, kRtpVideoPayloadType);
        EXPECT_TRUE(depacketizer.Add(p + 12, packet.size() - 12, annexB));
        if (p[1] & 0x80) break;
    }
    std::vector<std::string> nals;
    SplitAnnexB(annexB, nals);
    std::vector<uint8_t> types;
    for (const std::string& nal : nals)
        types.push_back(RtpPayloadFormat::For(VideoCodec::H264).NalType(nal));
    EXPECT_TRUE(Has(types, kNalSps) && Has(types, kNalPps) && Has(types, kNalIdrSlice));

    // Requests still work on the connection the stream is flowing on.
    std::string keepalive = Request("GET_PARAMETER", base, "Session: " + session + "\r\n");
    EXPECT_EQ(keepalive.rfind("RTSP/1.0 200 OK\r\n", 0), 0u) << keepalive;
}

TEST_F(FrameStreamTest, CountsTheAndroidAppApartFromOtherPlayers) {
    StartServer(StreamProtocol::RtspTcp);
    auto play = [&](const std::string& userAgent) {
        Connect();
        std::string setup = Request(
            "SETUP", base + "track1",
            "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\nUser-Agent: " + userAgent + "\r\n");
        std::string session = Header(setup, "Session");
        session = session.substr(0, session.find(';'));
        Request("PLAY", base, "Session: " + session + "\r\n");
    };
    play("LibVLC/3.0.20 (LIVE555 Streaming Media v2016.11.28)");
    EXPECT_EQ(server.Viewers(), 1);
    EXPECT_EQ(server.AppViewers(), 0);
    closesocket(rtsp);
    received.clear();
    play("Gazeora Android");
    EXPECT_EQ(server.AppViewers(), 1);
}

TEST_F(FrameStreamTest, CountsTheAndroidAppOverHttp) {
    StartServer(StreamProtocol::Http);
    Connect();
    std::string get =
        "GET /stream.ts HTTP/1.1\r\nHost: 127.0.0.1\r\nUser-Agent: Gazeora Android\r\n\r\n";
    send(rtsp, get.data(), int(get.size()), 0);
    WaitForViewers(1);
    for (int i = 0; i < 100 && server.AppViewers() != 1; ++i) Sleep(10);
    EXPECT_EQ(server.AppViewers(), 1);
}

TEST_F(FrameStreamTest, PlayWithoutSetupIsAnUnknownSession) {
    Connect();
    std::string response = Request("PLAY", base, "Session: 1234\r\n");
    EXPECT_EQ(response.rfind("RTSP/1.0 454 Session Not Found\r\n", 0), 0u) << response;
}

TEST_F(FrameStreamTest, StreamsIdleFrameWithoutLiveFrames) {
    server.SetIdleFrame(64, 48, Solid(64, 48, 0x40));
    Play();
    EXPECT_EQ(server.Viewers(), 1);
    EXPECT_TRUE(server.WantsFrames());
    // A new player gets a key frame first, with the parameter sets to decode it.
    std::vector<uint8_t> types;
    EXPECT_GT(ReceiveFrame(&types), 0);
    EXPECT_TRUE(Has(types, kNalSps) && Has(types, kNalPps) && Has(types, kNalIdrSlice));
    EXPECT_GT(ReceiveFrame(), 0);  // repeated
}

TEST_F(FrameStreamTest, KeepsTheIdleFrameWhenTheProtocolChanges) {
    server.SetIdleFrame(64, 48, Solid(64, 48, 0x40));
    Sleep(100);                            // the encode thread takes it
    StartServer(StreamProtocol::RtspUdp);  // a restart, as a change in Settings does
    Play();
    EXPECT_GT(ReceiveFrame(), 0);
}

TEST_F(FrameStreamTest, StreamsLiveFramesOverUdp) {
    Play();
    std::vector<uint8_t> frame = Solid(320, 240, 0x80);
    server.PushFrame(320, 240, frame);
    std::vector<uint8_t> types;
    EXPECT_GT(ReceiveFrame(&types), 0);
    EXPECT_TRUE(Has(types, kNalIdrSlice));
    EXPECT_TRUE(server.LastError().empty()) << Utf8::FromWide(server.LastError());
    EXPECT_FALSE(server.EncoderName().empty());
}

TEST_F(FrameStreamTest, TeardownStopsTheStream) {
    std::string session = Play();
    ASSERT_EQ(server.Viewers(), 1);
    std::string response = Request("TEARDOWN", base, "Session: " + session + "\r\n");
    EXPECT_EQ(response.rfind("RTSP/1.0 200 OK\r\n", 0), 0u) << response;
    EXPECT_EQ(server.Viewers(), 0);
    EXPECT_FALSE(server.WantsFrames());
}

TEST_F(FrameStreamTest, ClosingTheConnectionStopsTheStream) {
    Play();
    ASSERT_EQ(server.Viewers(), 1);
    closesocket(rtsp);
    rtsp = INVALID_SOCKET;
    for (int i = 0; i < 300 && server.Viewers() != 0; ++i) Sleep(10);
    EXPECT_EQ(server.Viewers(), 0);
}

TEST_F(FrameStreamTest, StopDisconnectsPlayers) {
    Play();
    server.Stop();
    EXPECT_FALSE(server.Running());
    EXPECT_EQ(server.Viewers(), 0);
    EXPECT_TRUE(server.Urls().empty());
    char byte;
    EXPECT_LE(recv(rtsp, &byte, 1, 0), 0);
}

TEST_F(FrameStreamTest, SpreadsALargeFrameOverSeveralMilliseconds) {
    Play();
    // Noise, so the key frame is large: dozens of packets.
    std::vector<uint8_t> frame(1280 * 720 * 4);
    uint32_t seed = 1;
    for (uint8_t& byte : frame) byte = uint8_t((seed = seed * 1664525u + 1013904223u) >> 24);
    server.PushFrame(1280, 720, frame);

    LARGE_INTEGER freq, first = {}, last = {};
    QueryPerformanceFrequency(&freq);
    char packet[2048];
    int packets = 0;
    for (int i = 0; i < 10000; ++i) {
        if (recv(rtp, packet, sizeof(packet), 0) <= 0) break;
        QueryPerformanceCounter(&last);
        if (packets++ == 0) first = last;
        if (packet[1] & 0x80) break;
    }
    ASSERT_GE(packets, 32);
    // Spread over half a frame interval (PacingPlan): about 8 ms at 60 fps; all at once would
    // take well under a millisecond.
    double spreadMs = double(last.QuadPart - first.QuadPart) * 1000.0 / double(freq.QuadPart);
    EXPECT_GE(spreadMs, 6.0) << packets << " packets";
    EXPECT_LT(spreadMs, 20.0) << packets << " packets";
}

TEST_F(FrameStreamTest, BacksOffWhenPlayersReportLossAndRecoversLater) {
    Play();
    EXPECT_EQ(server.Bitrate(), server.FullBitrate());

    // An RTCP receiver report for the server's stream: 10% of packets lost.
    SOCKET rtcp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    uint8_t report[32] = {0x81, 201, 0, 7};
    uint32_t words[] = {0x12345678, ssrc, (26u << 24) | 100, 0, 0, 0, 0};
    for (int i = 0; i < 7; ++i) {
        for (int b = 0; b < 4; ++b) report[4 + i * 4 + b] = uint8_t(words[i] >> (24 - 8 * b));
    }
    sockaddr_in to = Loopback(serverRtcpPort);
    // Players report every few seconds; the server backs off one step every 1.5 s.
    for (int i = 0; i < 40 && server.Bitrate() == server.FullBitrate(); ++i) {
        sendto(rtcp, reinterpret_cast<char*>(report), sizeof(report), 0,
               reinterpret_cast<sockaddr*>(&to), sizeof(to));
        Sleep(100);
    }
    closesocket(rtcp);
    EXPECT_NEAR(server.PacketLoss(), 26 / 256.0f, 0.001f);
    EXPECT_LT(server.Bitrate(), server.FullBitrate());

    // Once the player is gone, the next one starts at the full bitrate again.
    closesocket(this->rtsp);
    this->rtsp = INVALID_SOCKET;
    for (int i = 0; i < 300 && server.Bitrate() < server.FullBitrate(); ++i) Sleep(10);
    EXPECT_EQ(server.Bitrate(), server.FullBitrate());
}

TEST_F(FrameStreamTest, IgnoresLossReportsWithNegativeTotals) {
    Play();
    // VLC's first report: 252/256 lost lately, -1 lost in all.
    SOCKET rtcp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    uint8_t report[32] = {0x81, 201, 0, 7};
    uint32_t words[] = {0x12345678, ssrc, (252u << 24) | 0xFFFFFF, 0, 0, 0, 0};
    for (int i = 0; i < 7; ++i) {
        for (int b = 0; b < 4; ++b) report[4 + i * 4 + b] = uint8_t(words[i] >> (24 - 8 * b));
    }
    sockaddr_in to = Loopback(serverRtcpPort);
    for (int i = 0; i < 20; ++i) {
        sendto(rtcp, reinterpret_cast<char*>(report), sizeof(report), 0,
               reinterpret_cast<sockaddr*>(&to), sizeof(to));
        Sleep(100);
    }
    closesocket(rtcp);
    EXPECT_EQ(server.PacketLoss(), 0.0f);
    EXPECT_EQ(server.Bitrate(), server.FullBitrate());
}

TEST_F(FrameStreamTest, StreamsH265WhenChosenAndAvailable) {
    server.SetCodec(VideoCodec::H265);
    if (!server.CodecAvailable(VideoCodec::H265)) {
        EXPECT_EQ(server.Codec(), VideoCodec::H264);  // stays on what works
        GTEST_SKIP() << "no H.265 encoder on this machine";
    }
    EXPECT_EQ(server.Codec(), VideoCodec::H265);
    Connect();
    std::string describe = Request("DESCRIBE", base);
    EXPECT_NE(describe.find("a=rtpmap:96 H265/90000\r\n"), std::string::npos) << describe;
    closesocket(rtsp);
    rtsp = INVALID_SOCKET;

    Play();
    std::vector<uint8_t> frame = Solid(320, 240, 0x80);
    server.PushFrame(320, 240, frame);
    char packet[2048];
    RtpVideoDepacketizer depacketizer(VideoCodec::H265);
    std::string annexB;
    for (int i = 0; i < 10000; ++i) {
        int got = recv(rtp, packet, sizeof(packet), 0);
        ASSERT_GT(got, 12);
        const auto* p = reinterpret_cast<const uint8_t*>(packet);
        EXPECT_TRUE(depacketizer.Add(p + 12, size_t(got) - 12, annexB));
        if (p[1] & 0x80) break;
    }
    std::vector<std::string> nals;
    SplitAnnexB(annexB, nals);
    bool vps = false, sps = false, pps = false, key = false;
    for (const std::string& nal : nals) {
        vps |= RtpPayloadFormat::For(VideoCodec::H265).NalType(nal) == kHevcVps;
        sps |= RtpPayloadFormat::For(VideoCodec::H265).NalType(nal) == kHevcSps;
        pps |= RtpPayloadFormat::For(VideoCodec::H265).NalType(nal) == kHevcPps;
        key |= RtpPayloadFormat::For(VideoCodec::H265).IsKeyFrame(nal);
    }
    EXPECT_TRUE(vps && sps && pps && key);
    // Players that join later get the parameter sets in the SDP as well.
    Connect();
    describe = Request("DESCRIBE", base);
    EXPECT_NE(describe.find("a=fmtp:96 sprop-vps="), std::string::npos) << describe;
}

TEST_F(FrameStreamTest, ChangingTheCodecDisconnectsPlayers) {
    if (!server.CodecAvailable(VideoCodec::H265)) {
        GTEST_SKIP() << "no H.265 encoder on this machine";
    }
    Play();
    ASSERT_EQ(server.Viewers(), 1);
    server.SetCodec(VideoCodec::H265);
    char byte;
    EXPECT_LE(recv(rtsp, &byte, 1, 0), 0) << "the RTSP connection closes";
    for (int i = 0; i < 100 && server.Viewers() != 0; ++i) Sleep(10);
    EXPECT_EQ(server.Viewers(), 0);
    // The same codec again changes nothing.
    Play();
    server.SetCodec(VideoCodec::H265);
    Sleep(400);
    EXPECT_EQ(server.Viewers(), 1);
}

TEST_F(FrameStreamTest, SecondServerOnSamePortFails) {
    FrameStreamServer other{SoftwareOnlyEncoder::Create};
    other.SetDiscoveryPort(0);
    std::wstring error;
    EXPECT_FALSE(other.Start(server.Port(), false, error));
    EXPECT_EQ(error, L"port " + std::to_wstring(server.Port()) + L" is already in use");
    EXPECT_FALSE(other.Running());
}

TEST_F(FrameStreamTest, UrlsNameTheProtocol) {
    StartServer(StreamProtocol::Http);
    std::vector<std::wstring> urls = server.Urls();
    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], L"http://127.0.0.1:" + std::to_wstring(server.Port()) + L"/stream.ts");
    StartServer(StreamProtocol::Udp);
    urls = server.Urls();
    ASSERT_EQ(urls.size(), 1u);
    EXPECT_EQ(urls[0], L"udp://127.0.0.1:" + std::to_wstring(server.Port()));
}

TEST_F(FrameStreamTest, StreamsFramesOverUdpToPlayersThatSayHello) {
    StartServer(StreamProtocol::Udp);
    SOCKET player = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SetTimeout(player, 3000);
    sockaddr_in to = Loopback(server.Port());
    auto say = [&](FrameMessage type, uint32_t cookie) {
        FrameHeader header;
        header.type = type;
        header.timestamp = cookie;
        uint8_t raw[kFrameHeaderSize];
        header.Write(raw);
        sendto(player, reinterpret_cast<char*>(raw), sizeof(raw), 0,
               reinterpret_cast<sockaddr*>(&to), sizeof(to));
    };
    // A first hello only gets a challenge, no bigger than it: a forged one gains nothing.
    say(FrameMessage::Hello, 0);
    char reply[2048];
    int replied = recv(player, reply, sizeof(reply), 0);
    ASSERT_EQ(replied, int(kFrameHeaderSize));
    FrameHeader challenge;
    ASSERT_TRUE(challenge.Read(reinterpret_cast<uint8_t*>(reply), size_t(replied)));
    EXPECT_EQ(challenge.type, FrameMessage::Challenge);
    const uint32_t cookie = challenge.timestamp;
    EXPECT_NE(cookie, 0u);
    EXPECT_EQ(server.Viewers(), 0);
    // The cookie back, and it's a player.
    say(FrameMessage::Hello, cookie);
    WaitForViewers(1);
    ASSERT_EQ(server.Viewers(), 1);
    EXPECT_EQ(server.AppViewers(), 1);  // only the app speaks GZF1

    std::vector<uint8_t> frame = Solid(320, 240, 0x80);
    server.PushFrame(320, 240, frame);
    // The frame's chunks, in order: the first frame a player gets is a key frame.
    std::string annexB;
    FrameHeader header;
    for (uint32_t chunk = 0, count = 1; chunk < count; ++chunk) {
        char datagram[2048];
        int got = recv(player, datagram, sizeof(datagram), 0);
        ASSERT_GT(got, int(kFrameHeaderSize));
        ASSERT_TRUE(header.Read(reinterpret_cast<uint8_t*>(datagram), size_t(got)));
        EXPECT_EQ(header.type, FrameMessage::Frame);
        EXPECT_EQ(header.tail >> 16, chunk);
        count = header.tail & 0xFFFF;
        annexB.append(datagram + kFrameHeaderSize, size_t(got) - kFrameHeaderSize);
    }
    EXPECT_TRUE(header.flags & kFrameFlagKey);
    EXPECT_FALSE(header.flags & kFrameFlagH265);
    EXPECT_EQ(header.width, 320);
    EXPECT_EQ(header.height, 240);
    std::vector<std::string> nals;
    SplitAnnexB(annexB, nals);
    std::vector<uint8_t> types;
    for (const std::string& nal : nals)
        types.push_back(RtpPayloadFormat::For(VideoCodec::H264).NalType(nal));
    EXPECT_TRUE(Has(types, kNalSps) && Has(types, kNalPps) && Has(types, kNalIdrSlice));

    say(FrameMessage::Bye, cookie + 1);  // not with the wrong cookie
    Sleep(100);
    EXPECT_EQ(server.Viewers(), 1);
    say(FrameMessage::Bye, cookie);
    WaitForViewers(0);
    EXPECT_EQ(server.Viewers(), 0);
    closesocket(player);
}

TEST_F(FrameStreamTest, StreamsMpegTsOverHttp) {
    StartServer(StreamProtocol::Http);
    Connect();
    std::string get = "GET /stream.ts HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    send(rtsp, get.data(), int(get.size()), 0);
    while (received.find("\r\n\r\n") == std::string::npos && ReceiveMore()) {
    }
    size_t headEnd = received.find("\r\n\r\n");
    ASSERT_NE(headEnd, std::string::npos) << received;
    EXPECT_EQ(received.rfind("HTTP/1.1 200 OK\r\n", 0), 0u) << received;
    EXPECT_NE(received.find("Content-Type: video/mp2t\r\n"), std::string::npos) << received;
    // Web pages elsewhere can't read the stream.
    EXPECT_EQ(received.find("Access-Control-Allow-Origin"), std::string::npos) << received;
    received.erase(0, headEnd + 4);
    WaitForViewers(1);
    ASSERT_EQ(server.Viewers(), 1);
    EXPECT_EQ(server.AppViewers(), 0);  // no User-Agent: some other player

    std::vector<uint8_t> frame = Solid(320, 240, 0x80);
    server.PushFrame(320, 240, frame);
    // A player starts on a key frame, with the PAT and PMT in front.
    while (received.size() < 4 * TsMuxer::kPacketSize && ReceiveMore()) {
    }
    ASSERT_GE(received.size(), 4 * TsMuxer::kPacketSize);
    for (size_t at = 0; at + TsMuxer::kPacketSize <= received.size(); at += TsMuxer::kPacketSize) {
        EXPECT_EQ(uint8_t(received[at]), 0x47) << "sync byte of packet at " << at;
    }
    auto pid = [&](size_t packet) {
        size_t at = packet * TsMuxer::kPacketSize;
        return uint16_t((uint8_t(received[at + 1]) & 0x1F) << 8 | uint8_t(received[at + 2]));
    };
    EXPECT_EQ(pid(0), 0);  // PAT
    EXPECT_EQ(pid(1), TsMuxer::kPmtPid);
    EXPECT_EQ(pid(2), TsMuxer::kVideoPid);
    EXPECT_TRUE(uint8_t(received[2 * TsMuxer::kPacketSize + 5]) & 0x40) << "random access";
}

TEST_F(FrameStreamTest, HttpAnswersOtherPathsWithNotFound) {
    StartServer(StreamProtocol::Http);
    Connect();
    std::string get = "GET /other HTTP/1.1\r\n\r\n";
    send(rtsp, get.data(), int(get.size()), 0);
    while (ReceiveMore()) {
    }
    EXPECT_EQ(received.rfind("HTTP/1.1 404 Not Found\r\n", 0), 0u) << received;
}

TEST_F(FrameStreamTest, AnswersTheAndroidAppsSearch) {
    StartServer(StreamProtocol::Http);
    ASSERT_NE(server.DiscoveryPort(), 0);
    SOCKET app = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SetTimeout(app, 3000);
    sockaddr_in to = Loopback(server.DiscoveryPort());
    sendto(app, "not for us", 10, 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
    sendto(app, kDiscoveryRequest, int(sizeof(kDiscoveryRequest) - 1), 0,
           reinterpret_cast<sockaddr*>(&to), sizeof(to));
    char reply[1024];
    int got = recv(app, reply, sizeof(reply), 0);
    ASSERT_GT(got, 0);
    std::string json(reply, size_t(got));
    EXPECT_NE(json.find("\"service\":\"gazeora\""), std::string::npos) << json;
    EXPECT_NE(json.find("\"protocol\":\"http\""), std::string::npos) << json;
    EXPECT_NE(json.find("\"port\":" + std::to_string(server.Port()) + ","), std::string::npos)
        << json;
    EXPECT_NE(json.find("\"path\":\"/stream.ts\""), std::string::npos) << json;
    EXPECT_NE(json.find("\"codec\":\"h264\""), std::string::npos) << json;
    closesocket(app);
}

TEST(FrameStreamServerTest, KeepsTheFrameRateInRange) {
    FrameStreamServer server;
    EXPECT_EQ(server.FrameRate(), FrameStreamServer::kDefaultFrameRate);
    server.SetFrameRate(30);
    EXPECT_EQ(server.FrameRate(), 30u);
    server.SetFrameRate(0);
    EXPECT_EQ(server.FrameRate(), FrameStreamServer::kMinFrameRate);
    server.SetFrameRate(1000);
    EXPECT_EQ(server.FrameRate(), FrameStreamServer::kMaxFrameRate);
}

}  // namespace
}  // namespace gazeora
