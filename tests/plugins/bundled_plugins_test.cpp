// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <gtest/gtest.h>
#include <winsock2.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "gazeora/common/text/utf8.h"
#include "gazeora/host/stream/frame_packet.h"
#include "gazeora/host/stream/frame_stream.h"
#include "gazeora/host/stream/http_ts_output.h"
#include "gazeora/plugins/plugin_services.h"
#include "plugins/udp_player.h"

namespace gazeora {
namespace {

constexpr int kFrameSide = 64;
constexpr uint8_t kGray = 0x80;
constexpr size_t kBytesPerPixel = 4;
constexpr int kWaitSteps = 200;
constexpr DWORD kWaitStepMs = 10;

// The bundled plugins, as the build puts them next to Gazeora.exe (plugins/CMakeLists.txt).
PluginFolder BundledPlugins() { return PluginFolder(Utf8::ToWide(GAZEORA_BUNDLED_PLUGIN_DIR)); }

std::vector<std::string> IdsOf(const StreamProtocolCatalog& catalog) {
    std::vector<std::string> ids;
    for (const StreamProtocolInfo& info : catalog.All()) {
        ids.push_back(info.id.Id());
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

class BundledPluginsTest : public ::testing::Test {
protected:
    void SetUp() override {
        WSADATA wsa;
        ASSERT_EQ(WSAStartup(MAKEWORD(2, 2), &wsa), 0);
    }
    void TearDown() override {
        server.Stop();
        WSACleanup();
    }

    void Start(const StreamProtocol& protocol) {
        server.SetProtocol(protocol);
        server.SetDiscoveryPort(0);
        std::wstring error;
        ASSERT_TRUE(server.Start(0, false, error)) << error;
    }

    // Pushes frames until the player gets one; its header in `frame`.
    bool FrameReaches(UdpPlayer& player, FrameHeader& frame) {
        for (int i = 0; i < kWaitSteps; ++i) {
            PushGrayFrame();
            if (player.Receive(frame) && frame.type == FrameMessage::Frame) {
                return true;
            }
        }
        return false;
    }

    // PushFrame takes the buffer, so every frame is a new one.
    void PushGrayFrame() {
        std::vector<uint8_t> bgra(kFrameSide * kFrameSide * kBytesPerPixel, kGray);
        server.PushFrame(kFrameSide, kFrameSide, bgra);
    }

    void WaitForViewers(int viewers) {
        for (int i = 0; i < kWaitSteps && server.Viewers() != viewers; ++i) Sleep(kWaitStepMs);
    }

    PluginServices services{BundledPlugins()};
    FrameStreamServer server{services.Encoders(), services.Outputs(), services.Protocols(),
                             services.Codecs()};
};

TEST_F(BundledPluginsTest, EveryBundledPluginLoads) {
    ASSERT_EQ(services.Plugins().size(), 4u);
    std::vector<std::wstring> names;
    for (const PluginStatus& plugin : services.Plugins()) {
        EXPECT_TRUE(plugin.loaded) << plugin.path << L": " << plugin.error;
        EXPECT_TRUE(plugin.error.empty()) << plugin.error;
        names.push_back(plugin.name);
    }
    // In file order; the same names bundled.json gives the Plugins page (plugins/CMakeLists.txt).
    EXPECT_EQ(names, (std::vector<std::wstring>{L"H.265 (HEVC)", L"HTTP Live", L"UDP Direct",
                                                L"USB Cable"}));
    EXPECT_EQ(IdsOf(*services.Protocols()),
              (std::vector<std::string>{"http", "rtsp-tcp", "rtsp-udp", "udp", "usb"}));
    const VideoCodecInfo* h265 = services.Codecs()->Find(VideoCodec::H265);
    ASSERT_NE(h265, nullptr);
    EXPECT_EQ(h265->name, "H.265");
    EXPECT_EQ(h265->format->FrameFlags(), kFrameFlagH265);
}

TEST_F(BundledPluginsTest, ProtocolsKeepWhatTheDiscoveryReplySays) {
    const StreamProtocolInfo* http = services.Protocols()->Find(StreamProtocol::Http);
    ASSERT_NE(http, nullptr);
    EXPECT_EQ(http->discoveryPath, HttpTsOutput::Info().discoveryPath);
    EXPECT_TRUE(http->openedByPlayers);
    EXPECT_FALSE(services.Protocols()->Find(StreamProtocol::Udp)->openedByPlayers);
}

TEST_F(BundledPluginsTest, StreamsToTheAppOverTheUdpPlugin) {
    Start(StreamProtocol::Udp);
    EXPECT_EQ(server.Urls().at(0), L"udp://127.0.0.1:" + std::to_wstring(server.Port()));
    UdpPlayer player(server.Port());
    ASSERT_TRUE(player.Join());
    WaitForViewers(1);
    EXPECT_EQ(server.Viewers(), 1);
    EXPECT_EQ(server.AppViewers(), 1);
    FrameHeader frame;
    ASSERT_TRUE(FrameReaches(player, frame));
    EXPECT_TRUE(frame.flags & kFrameFlagKey);
    EXPECT_FALSE(frame.flags & kFrameFlagH265);
}

TEST_F(BundledPluginsTest, MarksH265FromThePluginInTheFrameHeader) {
    Start(StreamProtocol::Udp);  // which finds the encoders
    server.SetCodec(VideoCodec::H265);
    if (!server.CodecAvailable(VideoCodec::H265)) {
        GTEST_SKIP() << "no H.265 encoder on this PC";
    }
    EXPECT_EQ(server.Codec(), VideoCodec::H265);
    UdpPlayer player(server.Port());
    ASSERT_TRUE(player.Join());
    FrameHeader frame;
    ASSERT_TRUE(FrameReaches(player, frame));
    EXPECT_TRUE(frame.flags & kFrameFlagH265);
}

TEST_F(BundledPluginsTest, ServesTheHttpPluginsTransportStream) {
    Start(StreamProtocol::Http);
    SOCKET player = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    DWORD timeout = UdpPlayer::kReceiveTimeoutMs;
    setsockopt(player, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(server.Port());
    ASSERT_EQ(connect(player, reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);
    const std::string request = "GET /stream.ts HTTP/1.1\r\nHost: test\r\n\r\n";
    send(player, request.data(), int(request.size()), 0);
    WaitForViewers(1);
    std::string received;
    char chunk[4096];
    for (int i = 0; i < kWaitSteps && received.find("\r\n\r\nG") == std::string::npos; ++i) {
        PushGrayFrame();
        const int got = recv(player, chunk, sizeof(chunk), 0);
        if (got <= 0) break;
        received.append(chunk, size_t(got));
    }
    closesocket(player);
    EXPECT_EQ(received.rfind("HTTP/1.1 200", 0), 0u) << received.substr(0, 64);
    EXPECT_NE(received.find("\r\n\r\nG"), std::string::npos);  // 0x47: a TS packet's sync byte
}

}  // namespace
}  // namespace gazeora
