// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <windows.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "gazeora/common/text/utf8.h"
#include "gazeora/host/stream/frame_stream.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugins/platform/win32_plugin_module_loader.h"
#include "gazeora/plugins/plugin_services.h"
#include "gazeora/plugins/services/plugin_loader.h"

namespace gazeora {
namespace {

// The sample plugin DLLs, built into a folder of their own (tests/CMakeLists.txt).
PluginFolder TestPlugins() { return PluginFolder(Utf8::ToWide(GAZEORA_TEST_PLUGIN_DIR)); }

const PluginStatus* Find(const PluginServices& services, const std::wstring& file) {
    for (const PluginStatus& plugin : services.Plugins()) {
        if (plugin.path.size() >= file.size() &&
            plugin.path.compare(plugin.path.size() - file.size(), file.size(), file) == 0) {
            return &plugin;
        }
    }
    return nullptr;
}

TEST(PluginDllTest, LoadsTheSamplePlugin) {
    PluginServices services(TestPlugins());
    const PluginStatus* plugin = Find(services, L"gazeora_test_plugin.dll");
    ASSERT_NE(plugin, nullptr);
    EXPECT_TRUE(plugin->loaded) << plugin->error;
    EXPECT_EQ(plugin->name, L"Sample plugin");
    EXPECT_EQ(plugin->version, L"1.0.0");
    EXPECT_EQ(plugin->gameStores, 1);
    EXPECT_EQ(plugin->encoders, 1);
    EXPECT_EQ(plugin->codecs, 1);
    EXPECT_EQ(plugin->protocols, 1);
    EXPECT_EQ(plugin->gamepadDrivers, 1);
    EXPECT_EQ(plugin->uiPanels, 1);
    EXPECT_TRUE(plugin->error.empty()) << plugin->error;
}

TEST(PluginDllTest, ShowsThePluginsCard) {
    PluginServices services(TestPlugins());
    ASSERT_EQ(services.UiPanels().size(), 1u);
    const ui::UiPanel& panel = services.UiPanels()[0];
    EXPECT_EQ(panel.layout.page, ui::UiPage::Settings);
    EXPECT_EQ(panel.layout.title, "Sample plugin");
    EXPECT_FALSE(panel.id.empty());
    EXPECT_EQ(panel.values->Get("loud"), "0");
    panel.values->Set("loud", "1");
    EXPECT_EQ(panel.values->Get("loud"), "1");
    panel.values->Set("loud", "0");
    panel.values->Act("beep");
    EXPECT_EQ(panel.values->Get("loud"), "0");
}

TEST(PluginDllTest, ReadsControllersThroughThePluginsDriver) {
    PluginServices services(TestPlugins());
    ASSERT_NE(services.GamepadDrivers(), nullptr);
    std::unique_ptr<input::IGamepadDriver> driver = services.GamepadDrivers()->Create();
    driver->Update();
    ASSERT_EQ(driver->Connected().size(), 1u);
    EXPECT_EQ(driver->Connected()[0].name, L"Sample controller");
    const input::GamepadState state = driver->State();
    EXPECT_TRUE(state.IsDown(input::GamepadButton::South));
    EXPECT_FALSE(state.IsDown(input::GamepadButton::East));
    EXPECT_EQ(state.Axis(input::GamepadAxis::LeftX), 1.0f);
}

TEST(PluginDllTest, RefusesAPluginForAnotherApiVersion) {
    PluginServices services(TestPlugins());
    const PluginStatus* plugin = Find(services, L"gazeora_test_plugin_v99.dll");
    ASSERT_NE(plugin, nullptr);
    EXPECT_FALSE(plugin->loaded);
    EXPECT_NE(plugin->error.find(L"version 99"), std::wstring::npos) << plugin->error;
}

TEST(PluginDllTest, PluginStoreJoinsTheBuiltInOnes) {
    PluginServices services(TestPlugins());
    std::shared_ptr<const GameStore> store = services.Stores()->Find("sample");
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(store->Name(), L"Sample store");
    EXPECT_EQ(store->AppIdFor(L"C:\\Games\\sample_game.exe"), L"s-1");
    EXPECT_EQ(store->AppIdFor(L"C:\\Games\\other.exe"), L"");
    std::wstring error;
    EXPECT_TRUE(store->Launch(L"s-1", error)) << error;
    EXPECT_FALSE(store->Launch(L"x", error));
    EXPECT_EQ(error, L"unknown game");
    EXPECT_NE(services.Stores()->Find("steam"), nullptr);
}

TEST(PluginDllTest, PluginEncoderIsTriedFirst) {
    PluginServices services(TestPlugins());
    std::unique_ptr<VideoEncoder> encoder = services.Encoders()->Create();
    VideoEncoder::Settings settings;
    settings.width = 64;
    settings.height = 64;
    std::wstring error;
    ASSERT_TRUE(encoder->Open(settings, error)) << error;
    EXPECT_EQ(encoder->Name(), L"Sample encoder");
    std::vector<uint8_t> nv12(64 * 64 * 3 / 2);
    std::vector<VideoEncoder::Frame> frames;
    ASSERT_TRUE(encoder->Encode(nv12.data(), 1234, frames, error)) << error;
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].annexB, std::string("\0\0\0\x01\x65\x88", 6));
    EXPECT_EQ(frames[0].time, 1234);
    EXPECT_TRUE(frames[0].keyFrame);
}

TEST(PluginDllTest, EncoderWithoutTextureInputRefusesADevice) {
    Win32PluginModuleLoader modules;
    PluginCatalog catalog =
        PluginLoader(modules).Load({TestPlugins().Path() + L"\\gazeora_test_plugin.dll"});
    ASSERT_EQ(catalog.encoderProviders.size(), 1u);
    std::unique_ptr<VideoEncoder> encoder = catalog.encoderProviders[0]->Create();
    VideoEncoder::Settings settings;
    settings.width = 64;
    settings.height = 64;
    // Never used: the adapter refuses before the plugin sees it.
    settings.device = reinterpret_cast<ID3D11Device*>(uintptr_t(1));
    std::wstring error;
    EXPECT_FALSE(encoder->Open(settings, error));
    EXPECT_EQ(error, L"Sample encoder takes no textures");
}

TEST(PluginDllTest, LoadsAPluginBuiltForVersion1) {
    PluginServices services(TestPlugins());
    const PluginStatus* plugin = Find(services, L"gazeora_test_plugin_v1.dll");
    ASSERT_NE(plugin, nullptr);
    EXPECT_TRUE(plugin->loaded) << plugin->error;
    EXPECT_EQ(plugin->name, L"Sample plugin (version 1)");
    EXPECT_EQ(plugin->gameStores, 1);
    EXPECT_EQ(plugin->encoders, 1);
    EXPECT_EQ(plugin->codecs, 0);
    EXPECT_EQ(plugin->protocols, 0);
}

TEST(PluginDllTest, PluginProtocolAndCodecJoinTheBuiltInOnes) {
    PluginServices services(TestPlugins());
    const StreamProtocol out("sample-out");
    const VideoCodec codec("sample-codec");
    const StreamProtocolInfo* protocol = services.Protocols()->Find(out);
    ASSERT_NE(protocol, nullptr);
    EXPECT_EQ(protocol->name, "Sample output");
    EXPECT_TRUE(services.Outputs()->Has(out));
    EXPECT_TRUE(services.Protocols()->Has(StreamProtocol::RtspTcp));
    const VideoCodecInfo* info = services.Codecs()->Find(codec);
    ASSERT_NE(info, nullptr);
    ASSERT_NE(info->format, nullptr);
    EXPECT_EQ(info->format->Codec(), codec);
    EXPECT_STREQ(info->format->Rtp().EncodingName(), "SAMPLE");
    EXPECT_TRUE(info->format->Rtp().IsKeyFrame("e"));
    EXPECT_TRUE(services.Encoders()->Available(codec));
    EXPECT_TRUE(services.Codecs()->Has(VideoCodec::H264));
}

TEST(PluginDllTest, PluginCodecPacketizesLikeTheBuiltInOnes) {
    PluginServices services(TestPlugins());
    const VideoCodecFormat& format = *services.Codecs()->Find(VideoCodec("sample-codec"))->format;
    const std::string big = "e" + std::string(3000, 'x');
    std::vector<std::string> packets;
    uint16_t seq = 0;
    format.Rtp().Packetize({big}, 0, 0, 1400, seq, packets);
    ASSERT_EQ(packets.size(), 3u);                  // cut into fragmentation units
    EXPECT_EQ(uint8_t(packets[0][12]), 0x60 | 28);  // the plugin's FU indicator
    EXPECT_EQ(uint8_t(packets[0][13]), 0x80 | 5);   // start bit and the NAL type
    EXPECT_EQ(uint8_t(packets[2][13]), 0x40 | 5);   // end bit
}

TEST(PluginDllTest, StreamsAPluginCodecThroughAPluginProtocol) {
    PluginServices services(TestPlugins());
    HMODULE plugin = GetModuleHandleW(L"gazeora_test_plugin.dll");
    ASSERT_NE(plugin, nullptr);
    auto framesSent =
        reinterpret_cast<int32_t(GAZEORA_CALL*)()>(GetProcAddress(plugin, "SampleFramesSent"));
    auto lastCodec =
        reinterpret_cast<const char*(GAZEORA_CALL*)()>(GetProcAddress(plugin, "SampleLastCodec"));
    ASSERT_TRUE(framesSent && lastCodec);

    FrameStreamServer server{services.Encoders(), services.Outputs(), services.Protocols(),
                             services.Codecs()};
    server.SetProtocol(StreamProtocol("sample-out"));
    server.SetCodec(VideoCodec("sample-codec"));
    server.SetDiscoveryPort(0);
    std::wstring error;
    ASSERT_TRUE(server.Start(0, false, error)) << error;
    EXPECT_EQ(server.Port(), 4321);
    EXPECT_EQ(server.Codec(), VideoCodec("sample-codec"));
    const std::vector<std::wstring> urls = server.Urls();
    ASSERT_FALSE(urls.empty());
    EXPECT_EQ(urls[0], L"sample://127.0.0.1/");

    const int before = framesSent();
    std::vector<uint8_t> bgra(64 * 64 * 4, 0x80);
    for (int i = 0; i < 200 && framesSent() == before; ++i) {
        server.PushFrame(64, 64, bgra);
        Sleep(25);
    }
    server.Stop();
    EXPECT_GT(framesSent(), before);
    EXPECT_STREQ(lastCodec(), "sample-codec");
}

TEST(PluginDllTest, MissingFolderMeansNoPlugins) {
    PluginServices services(PluginFolder(L"C:\\gazeora\\no_such_folder"));
    EXPECT_TRUE(services.Plugins().empty());
    EXPECT_NE(services.Stores()->Find("steam"), nullptr);
    EXPECT_TRUE(services.Encoders()->Available(VideoCodec::H264));
}

}  // namespace
}  // namespace gazeora
