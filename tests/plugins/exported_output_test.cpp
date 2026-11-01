// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <gtest/gtest.h>
#include <winsock2.h>

#include <memory>
#include <string>
#include <vector>

#include "gazeora/bundled_parts/bundled_video_codecs.h"
#include "gazeora/host/stream/frame_packet.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_kit/exports/exported_protocol.h"
#include "gazeora/plugins/services/plugin_loader.h"
#include "plugins/fake_plugin_module.h"
#include "plugins/fake_plugin_module_loader.h"
#include "plugins/recording_output.h"

namespace gazeora {
namespace {

constexpr uint16_t kAskedPort = 5000;
constexpr uint64_t kNowMs = 123456;

const GazeoraPluginInfoV1 kInfo = {sizeof(GazeoraPluginInfoV1), GAZEORA_PLUGIN_API_VERSION,
                                   "Recording plugin", "1.0"};

const GazeoraPluginInfoV1* GAZEORA_CALL Info() { return &kInfo; }

int32_t GAZEORA_CALL Register(const GazeoraPluginHostV1* host) {
    static const ExportedProtocol protocol({kRecording, "Recording", "For the tests."},
                                           std::make_shared<RecordingFactory>());
    const GazeoraPluginHostV2* v2 = GazeoraPluginHostV2Of(host);
    return v2 ? v2->register_protocol(host->context, &protocol.Protocol()) : GAZEORA_FALSE;
}

class Touches final : public PointerEventSink {
public:
    void OnPointer(const PointerEvent& event) override { events.push_back(event); }
    std::vector<PointerEvent> events;
};

class Losses final : public LossReportSink {
public:
    void Report(uint64_t source, float fractionLost, uint64_t nowMs) override {
        reports.push_back({source, fractionLost, nowMs});
    }
    struct Entry {
        uint64_t source;
        float fraction;
        uint64_t nowMs;
    };
    std::vector<Entry> reports;
};

in_addr Address(uint32_t hostOrder) {
    in_addr address = {};
    address.s_addr = htonl(hostOrder);
    return address;
}

// The recording output exported as a plugin's, then loaded back as Gazeora loads one.
class ExportedOutputTest : public ::testing::Test {
protected:
    void SetUp() override {
        FakeModuleLoader modules(std::make_shared<FakeModule>(std::map<std::string, void*>{
            {GAZEORA_PLUGIN_INFO_SYMBOL, reinterpret_cast<void*>(&Info)},
            {GAZEORA_PLUGIN_REGISTER_SYMBOL, reinterpret_cast<void*>(&Register)}}));
        catalog = PluginLoader(modules).Load({L"C:\\plugins\\recording.dll"});
        ASSERT_EQ(catalog.outputs.size(), 1u);
        output = catalog.outputs[0]->Create(log);
        output->SetPointerSink(&touches);
    }

    PluginCatalog catalog;
    std::vector<std::string> lines;
    StreamLog log{[this](const char* line) { lines.push_back(line); }};
    Touches touches;
    std::unique_ptr<FrameOutput> output;
};

TEST_F(ExportedOutputTest, StartsWithTheAddressAndPortAsked) {
    std::wstring error;
    ASSERT_TRUE(output->Start(Address(INADDR_LOOPBACK), kAskedPort, error)) << error;
    EXPECT_EQ(TheRecord().startedOn, htonl(INADDR_LOOPBACK));
    EXPECT_EQ(TheRecord().askedPort, kAskedPort);
    EXPECT_EQ(lines, (std::vector<std::string>{"recording on 5000"}));
    output->Stop();
    EXPECT_TRUE(TheRecord().stopped);
}

TEST_F(ExportedOutputTest, PassesOnTheOutputsError) {
    std::wstring error;
    EXPECT_FALSE(output->Start(Address(INADDR_LOOPBACK), kRefusedPort, error));
    EXPECT_EQ(error, L"port taken");
}

TEST_F(ExportedOutputTest, TouchesOnTheOutputsLinkReachGazeora) {
    std::wstring error;
    ASSERT_TRUE(output->Start(Address(INADDR_LOOPBACK), kAskedPort, error)) << error;
    ASSERT_EQ(touches.events.size(), 1u);
    EXPECT_EQ(touches.events[0].action, PointerAction::Press);
    EXPECT_NEAR(touches.events[0].x, kTouchX, 0.001f);
    EXPECT_NEAR(touches.events[0].y, kTouchY, 0.001f);
}

TEST_F(ExportedOutputTest, AnswersWhatTheServerAsks) {
    EXPECT_EQ(output->Port(), kPort);
    EXPECT_EQ(output->Viewers(), 2);
    EXPECT_EQ(output->AppViewers(), 1);
    EXPECT_TRUE(output->TakeKeyFrameRequest());
    EXPECT_EQ(output->Url(L"192.168.1.20"), L"rec://192.168.1.20");
    EXPECT_EQ(output->Status(), L"recording");
    EXPECT_TRUE(output->Watching(Address(INADDR_LOOPBACK)));
    EXPECT_FALSE(output->Watching(Address(INADDR_ANY)));
    EXPECT_TRUE(output->NeedsUsbDriver());
    const std::vector<UsbPhone> phones = output->PhonesNeedingDriver();
    ASSERT_EQ(phones.size(), 1u);
    EXPECT_EQ(phones[0].hardwareId, L"USB\\VID_1234&PID_5678");
    EXPECT_EQ(phones[0].name, L"Test phone");
}

TEST_F(ExportedOutputTest, SendsFramesWithTheirCodecAndReportsLoss) {
    std::wstring error;
    ASSERT_TRUE(output->Start(Address(INADDR_LOOPBACK), kAskedPort, error)) << error;
    const VideoCodecFormat& h265 = BundledVideoCodecs::FormatOf(VideoCodec::H265);
    const std::vector<std::string> packets;
    const std::string annexB("\0\0\0\1\x26\x01key", 9);
    const VideoPipeline::EncodedFrame frame{packets, annexB, 0,  true, VideoCodec::H265,
                                            h265,    64,     64, 0,    30};
    Losses losses;
    output->Send(frame, losses, kNowMs);
    EXPECT_EQ(TheRecord().codec, "h265");
    EXPECT_EQ(TheRecord().flags, kFrameFlagH265);
    EXPECT_EQ(TheRecord().tsStreamType, h265.TsStreamType());
    EXPECT_EQ(TheRecord().annexB, annexB);
    ASSERT_EQ(losses.reports.size(), 1u);
    EXPECT_EQ(losses.reports[0].source, kPlayer);
    EXPECT_FLOAT_EQ(losses.reports[0].fraction, kLoss);
    EXPECT_EQ(losses.reports[0].nowMs, kNowMs);
}

}  // namespace
}  // namespace gazeora
