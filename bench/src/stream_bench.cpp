// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// The stream server: the hand-off from the capture thread to the encode thread, the pacing of
// UDP packets on the send thread, and the whole server end to end at 30 and 60 fps, from PushFrame
// to a player on loopback receiving the frame's packets.
#include <gtest/gtest.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "bench_util.h"
#include "gazeora/host/stream/frame_mailbox.h"
#include "gazeora/host/stream/frame_stream.h"
#include "gazeora/host/stream/net.h"
#include "gazeora/host/stream/packet_send_queue.h"
#include "gazeora/host/stream/rtcp.h"

namespace gazeora::bench {
namespace {

TEST(Stream, FrameMailboxHandOff) {
    PrintHeader("FrameMailbox, 1080p frames every 4 ms");
    const Size size = k1080p;
    FrameMailbox mailbox;
    std::atomic<bool> stop{false};
    std::vector<double> latency;
    std::thread consumer([&] {
        Picture live, idle;
        while (!stop) {
            if (mailbox.Take(100, live, idle)) {
                latency.push_back(QpcSeconds() - double(live.clock) * 1e-9);
            }
        }
    });
    std::vector<uint8_t> bgra;
    std::vector<double> posts;
    Pacer pacer;
    double next = QpcSeconds();
    for (int i = 0; i < 300; ++i) {
        pacer.SleepUntil(next += 0.004);
        bgra.resize(size_t(size.width) * size.height * 4);  // a swapped-back buffer may be empty
        double start = QpcSeconds();
        mailbox.PostLive(size.width, size.height, uint64_t(start * 1e9), bgra);
        posts.push_back(QpcSeconds() - start);
    }
    stop = true;
    mailbox.Wake();
    consumer.join();
    Print("PostLive (capture thread)", Stats::Of(posts));
    Print("post to Take returning (encode thread)", Stats::Of(latency),
          std::to_string(latency.size()) + " of 300 taken");
}

TEST(Stream, PacketSendQueuePacing) {
    PrintHeader("PacketSendQueue, 20 Mbit/s, frames to loopback");
    net::WinsockRuntime winsock;
    std::wstring error;
    ASSERT_TRUE(winsock.Start(error)) << Narrow(error);
    SOCKET sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SOCKET receiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in target = {};
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int length = sizeof(target);
    ASSERT_EQ(bind(receiver, reinterpret_cast<sockaddr*>(&target), sizeof(target)), 0);
    getsockname(receiver, reinterpret_cast<sockaddr*>(&target), &length);
    PacketSendQueue queue;
    for (uint32_t fps : {30u, 60u}) {
        for (size_t count : {20, 78, 150}) {
            std::vector<std::string> packets(count, std::string(1400, 'x'));
            PacingPlan plan = PacingPlan::For(count, count * 1400, 20000000, fps);
            std::vector<double> enqueue;
            Stats s = Measure(30, [&] {
                double start = QpcSeconds();
                queue.Enqueue(sender, {target}, packets, plan);
                enqueue.push_back(QpcSeconds() - start);
                queue.Flush();
            });
            char name[64];
            snprintf(name, sizeof(name), "%zu packets at %u fps, sent in", count, fps);
            Print(name, s,
                  std::to_string(plan.Bursts()) + " bursts of " +
                      std::to_string(plan.PacketsPerBurst()) + ", planned " +
                      FormatSeconds(double(plan.SpreadUs()) / 1e6) + "; Enqueue " +
                      FormatSeconds(Stats::Of(enqueue).median));
        }
    }
    closesocket(sender);
    closesocket(receiver);
}

// Opens the stream like a player over UDP: DESCRIBE, SETUP, PLAY.
class UdpPlayer {
public:
    ~UdpPlayer() {
        for (SOCKET s : {m_rtsp, m_rtp, m_rtcp}) {
            if (s != INVALID_SOCKET) closesocket(s);
        }
    }

    bool Play(uint16_t serverPort, std::string& error) {
        m_rtsp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in server = Loopback(serverPort);
        if (connect(m_rtsp, reinterpret_cast<sockaddr*>(&server), sizeof(server)) != 0) {
            error = "connect failed";
            return false;
        }
        uint16_t port = BindPortPair();
        if (!port) {
            error = "no free pair of UDP ports";
            return false;
        }
        std::string base = "rtsp://127.0.0.1:" + std::to_string(serverPort) + "/";
        Request("DESCRIBE " + base, "");
        std::string setup =
            Request("SETUP " + base + "track1",
                    "Transport: RTP/AVP;unicast;client_port=" + std::to_string(port) + "-" +
                        std::to_string(port + 1) + "\r\n");
        size_t at = setup.find("\r\nSession: ");
        if (setup.rfind("RTSP/1.0 200", 0) != 0 || at == std::string::npos) {
            error = "SETUP failed: " + setup;
            return false;
        }
        std::string session = setup.substr(at + 11, setup.find_first_of(";\r", at + 11) - at - 11);
        std::string play = Request("PLAY " + base, "Session: " + session + "\r\n");
        if (play.rfind("RTSP/1.0 200", 0) != 0) {
            error = "PLAY failed: " + play;
            return false;
        }
        return true;
    }

    SOCKET Rtp() const { return m_rtp; }

private:
    static sockaddr_in Loopback(uint16_t port) {
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        return address;
    }

    // An RTP port with the RTCP port after it, both bound like a real player's.
    uint16_t BindPortPair() {
        for (int attempt = 0; attempt < 50; ++attempt) {
            if (m_rtp != INVALID_SOCKET) closesocket(m_rtp);
            m_rtp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in address = Loopback(0);
            int length = sizeof(address);
            if (bind(m_rtp, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) continue;
            getsockname(m_rtp, reinterpret_cast<sockaddr*>(&address), &length);
            uint16_t port = ntohs(address.sin_port);
            if (port == 65535) continue;
            m_rtcp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in next = Loopback(uint16_t(port + 1));
            if (bind(m_rtcp, reinterpret_cast<sockaddr*>(&next), sizeof(next)) == 0) {
                DWORD timeoutMs = 200;
                setsockopt(m_rtp, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeoutMs),
                           sizeof(timeoutMs));
                int buffer = 8 << 20;
                setsockopt(m_rtp, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&buffer),
                           sizeof(buffer));
                return port;
            }
            closesocket(m_rtcp);
            m_rtcp = INVALID_SOCKET;
        }
        return 0;
    }

    // Sends "<method> <url>" and returns the response's head (these carry no body we need,
    // but DESCRIBE's is read too so it doesn't run into the next response).
    std::string Request(const std::string& line, const std::string& headers) {
        std::string request =
            line + " RTSP/1.0\r\nCSeq: " + std::to_string(++m_cseq) + "\r\n" + headers + "\r\n";
        send(m_rtsp, request.data(), int(request.size()), 0);
        std::string received;
        char buffer[4096];
        while (true) {
            size_t headEnd = received.find("\r\n\r\n");
            if (headEnd != std::string::npos) {
                size_t lengthAt = received.find("Content-Length: ");
                size_t length =
                    lengthAt < headEnd ? strtoul(&received[lengthAt + 16], nullptr, 10) : 0;
                if (received.size() >= headEnd + 4 + length) return received;
            }
            int got = recv(m_rtsp, buffer, sizeof(buffer), 0);
            if (got <= 0) return received;
            received.append(buffer, size_t(got));
        }
    }

    SOCKET m_rtsp = INVALID_SOCKET;
    SOCKET m_rtp = INVALID_SOCKET;
    SOCKET m_rtcp = INVALID_SOCKET;  // holds the port; the server's sender reports land here
    int m_cseq = 0;
};

// Pushes 1080p frames at 30 fps into a server with one UDP player on loopback, and times
// each frame from PushFrame to its first and its last (marker) packet arriving. RTP
// timestamps are the push time on the 90 kHz clock, so frames are matched by them.
void BenchServer(VideoCodec codec, uint32_t fps) {
    const Size size = k1080p;
    constexpr int kFrames = 140;  // counted
    // Counting starts this many frames after the first one arrives: before, frames replace each
    // other while the GPU encoder opens (over a second), then comes the key frame.
    constexpr int kSkip = 10;
    constexpr int kMaxUncounted = 600;
    FrameStreamServer server;
    server.SetProtocol(StreamProtocol::RtspUdp);  // the player sets up RTP over UDP
    server.SetDiscoveryPort(0);                   // any free port, beside a running Gazeora
    std::wstring error;
    ASSERT_TRUE(server.Start(0, false, error)) << Narrow(error);
    if (!server.CodecAvailable(codec)) {
        PrintNote(CodecName(codec), "no encoder on this PC");
        return;
    }
    server.SetCodec(codec);
    server.SetFrameRate(fps);
    UdpPlayer player;
    std::string playError;
    ASSERT_TRUE(player.Play(server.Port(), playError)) << playError;
    for (int i = 0; i < 100 && server.Viewers() == 0; ++i) Sleep(10);
    ASSERT_EQ(server.Viewers(), 1);

    std::vector<std::vector<uint8_t>> scenes(16);
    for (size_t i = 0; i < scenes.size(); ++i)
        FillScene(scenes[i], size.width, size.height, int(i));

    std::atomic<bool> stop{false};
    std::atomic<uint32_t> firstCountedTimestamp{0};
    std::atomic<bool> counting{false};
    std::atomic<bool> flowing{false};
    std::vector<double> first, last;
    std::vector<size_t> packetCounts;
    std::thread receiver([&] {
        char packet[2048];
        uint32_t current = 0;
        bool inFrame = false;
        size_t packets = 0;
        while (!stop) {
            int got = recv(player.Rtp(), packet, sizeof(packet), 0);
            if (got < 12) continue;
            flowing = true;
            uint32_t now = uint32_t(RtpClock64());
            const auto* p = reinterpret_cast<const uint8_t*>(packet);
            uint32_t timestamp = uint32_t(p[4]) << 24 | p[5] << 16 | p[6] << 8 | p[7];
            bool counted = counting && int32_t(timestamp - firstCountedTimestamp) >= 0;
            if (!inFrame || timestamp != current) {
                current = timestamp;
                inFrame = true;
                packets = 0;
                if (counted) first.push_back(double(now - timestamp) / 90000.0);
            }
            ++packets;
            if (p[1] & 0x80) {
                if (counted) {
                    last.push_back(double(now - timestamp) / 90000.0);
                    packetCounts.push_back(packets);
                }
                inFrame = false;
            }
        }
    });

    Pacer pacer;
    std::vector<uint8_t> frame;
    double next = QpcSeconds();
    int sinceFlowing = 0, counted = 0;
    for (int i = 0; counted < kFrames && i < kMaxUncounted + kFrames; ++i) {
        pacer.SleepUntil(next += 1.0 / fps);
        frame = scenes[size_t(i) % scenes.size()];
        if (flowing && sinceFlowing++ == kSkip) {
            firstCountedTimestamp = uint32_t(RtpClock64());
            counting = true;
        }
        counted += counting;
        server.PushFrame(size.width, size.height, frame);
    }
    Sleep(300);
    stop = true;
    receiver.join();

    size_t packets = 0;
    for (size_t n : packetCounts) packets += n;
    std::string label =
        std::string(CodecName(codec)) + " " + size.name + " " + std::to_string(fps) + " fps";
    PrintNote(label, Narrow(server.EncoderName()) + ", " + std::to_string(last.size()) + " of " +
                         std::to_string(counted) + " frames arrived, " +
                         std::to_string(packetCounts.empty() ? 0 : packets / packetCounts.size()) +
                         " packets each");
    Print("  PushFrame to first packet", Stats::Of(first));
    Print("  PushFrame to whole frame", Stats::Of(last));
    EXPECT_TRUE(server.LastError().empty()) << Narrow(server.LastError());
    server.Stop();
}

TEST(Stream, ServerEndToEndOverUdp) {
    PrintHeader("FrameStreamServer, one UDP player on loopback");
    for (uint32_t fps : {30u, 60u}) {
        for (VideoCodec codec : {VideoCodec::H264, VideoCodec::H265}) BenchServer(codec, fps);
    }
}

}  // namespace
}  // namespace gazeora::bench
