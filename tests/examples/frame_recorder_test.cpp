// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <windows.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "gazeora/bundled_parts/bundled_video_codecs.h"
#include "gazeora/common/text/utf8.h"
#include "gazeora/host/stream/frame_output_registry.h"
#include "gazeora/plugins/plugin_services.h"

namespace gazeora {
namespace {

namespace fs = std::filesystem;

const StreamProtocol kRecorder("recorder");
const std::string kKeyFrame("\0\0\0\x01\x65key", 8);
// The NAL header byte on its own, or "\x41" would run on into "de".
const std::string kDelta = std::string("\0\0\0\x01\x41", 5) + "delta";

class NoLoss final : public LossReportSink {
public:
    void Report(uint64_t, float, uint64_t) override {}
};

std::string Utf8Of(const fs::path& path) { return Utf8::FromWide(path.wstring()); }

std::string Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// The example plugin (examples/plugins/frame_recorder) as Gazeora loads it, from the folder the
// build puts it in, with its settings and recordings in a temporary folder.
class FrameRecorderTest : public ::testing::Test {
protected:
    void SetUp() override {
        root = fs::path(::testing::TempDir()) /
               ("gazeora_frame_recorder_" + std::to_string(GetCurrentProcessId()));
        fs::remove_all(root);
        fs::create_directories(Settings());
        Load();
        ASSERT_EQ(services->UiPanels().size(), 1u);
        Card().Set("folder", Utf8Of(Videos()));
    }

    void TearDown() override {
        output.reset();
        services.reset();
        std::error_code error;
        fs::remove_all(root, error);
    }

    void Load() {
        output.reset();
        services.reset();
        services = std::make_unique<PluginServices>(
            PluginFolder(Utf8::ToWide(GAZEORA_EXAMPLE_PLUGIN_DIR)), Settings().wstring());
    }

    fs::path Settings() const { return root / "settings"; }
    fs::path Videos() const { return root / "videos"; }
    ui::IUiValues& Card() { return *services->UiPanels()[0].values; }

    void StartOutput() {
        output = services->Outputs()->Create(kRecorder, log);
        std::wstring error;
        ASSERT_TRUE(output->Start(in_addr{}, 0, error)) << error;
    }

    void Send(const std::string& annexB, bool keyFrame) {
        const std::vector<std::string> packets;
        const VideoCodecFormat& h264 = BundledVideoCodecs::FormatOf(VideoCodec::H264);
        const VideoPipeline::EncodedFrame frame{packets, annexB, 0,  keyFrame, VideoCodec::H264,
                                                h264,    64,     64, 0,        30};
        output->Send(frame, losses, 0);
    }

    std::vector<fs::path> Recordings() const {
        std::vector<fs::path> files;
        if (fs::exists(Videos())) {
            for (const fs::directory_entry& entry : fs::directory_iterator(Videos())) {
                files.push_back(entry.path());
            }
        }
        return files;
    }

    bool Logged(const std::string& text) const {
        for (const std::string& line : lines) {
            if (line.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    fs::path root;
    std::unique_ptr<PluginServices> services;
    std::vector<std::string> lines;
    StreamLog log{[this](const char* line) { lines.push_back(line); }};
    NoLoss losses;
    std::unique_ptr<FrameOutput> output;
};

TEST_F(FrameRecorderTest, LoadsAsAProtocolAndACard) {
    ASSERT_EQ(services->Plugins().size(), 1u);
    const PluginStatus& plugin = services->Plugins()[0];
    EXPECT_TRUE(plugin.loaded) << plugin.error;
    EXPECT_EQ(plugin.name, L"Frame recorder");
    EXPECT_EQ(plugin.version, L"1.0.0");
    EXPECT_EQ(plugin.protocols, 1);
    EXPECT_EQ(plugin.uiPanels, 1);
    const StreamProtocolInfo* protocol = services->Protocols()->Find(kRecorder);
    ASSERT_NE(protocol, nullptr);
    EXPECT_EQ(protocol->name, "Record to file");
    const ui::UiPanel& panel = services->UiPanels()[0];
    EXPECT_EQ(panel.layout.page, ui::UiPage::Settings);
    EXPECT_EQ(panel.layout.title, "Frame recorder");
    EXPECT_EQ(Card().Get("recording"), "1");
}

TEST_F(FrameRecorderTest, RecordsFromTheFirstKeyFrame) {
    StartOutput();
    EXPECT_TRUE(output->TakeKeyFrameRequest());
    EXPECT_FALSE(output->TakeKeyFrameRequest());
    EXPECT_EQ(output->Viewers(), 0);
    EXPECT_EQ(output->Url(L"127.0.0.1"), L"");

    Send(kDelta, false);  // a file can't start here: it wouldn't play
    EXPECT_TRUE(Recordings().empty());
    Send(kKeyFrame, true);
    Send(kDelta, false);
    EXPECT_EQ(Card().Get("tone"), "success");
    EXPECT_NE(Card().Get("status").find("2 frames"), std::string::npos) << Card().Get("status");
    EXPECT_EQ(Card().Get("file-open"), "1");
    EXPECT_TRUE(Logged("recording to ")) << lines.size();
    output->Stop();

    const std::vector<fs::path> files = Recordings();
    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0].extension(), ".h264");
    EXPECT_EQ(Read(files[0]), kKeyFrame + kDelta);
}

TEST_F(FrameRecorderTest, StartsAFileAfterTheAccessUnitDelimiter) {
    // As Gazeora's frames come; VLC doesn't recognize a file that starts with one.
    const std::string delimiter("\0\0\0\x01\x09\xf0", 6);
    StartOutput();
    Send(delimiter + kKeyFrame, true);
    Send(delimiter + kDelta, false);
    output->Stop();
    ASSERT_EQ(Recordings().size(), 1u);
    EXPECT_EQ(Read(Recordings()[0]), kKeyFrame + delimiter + kDelta);
}

TEST_F(FrameRecorderTest, TurningItOffEndsTheFile) {
    StartOutput();
    Send(kKeyFrame, true);
    Card().Set("recording", "0");
    EXPECT_EQ(Card().Get("status"), "Off.");
    EXPECT_EQ(Card().Get("file-open"), "0");
    Send(kKeyFrame, true);
    ASSERT_EQ(Recordings().size(), 1u);
    EXPECT_EQ(Read(Recordings()[0]), kKeyFrame);

    output->TakeKeyFrameRequest();
    Card().Set("recording", "1");
    EXPECT_TRUE(output->TakeKeyFrameRequest());
    Send(kKeyFrame, true);
    output->Stop();
    EXPECT_EQ(Recordings().size(), 2u);
}

TEST_F(FrameRecorderTest, StartsANewFileAtTheNextKeyFrame) {
    StartOutput();
    Send(kKeyFrame, true);
    output->TakeKeyFrameRequest();
    Card().Act("new-file");
    EXPECT_TRUE(output->TakeKeyFrameRequest());
    Send(kDelta, false);
    Send(kKeyFrame, true);
    output->Stop();
    const std::vector<fs::path> files = Recordings();
    ASSERT_EQ(files.size(), 2u);
    EXPECT_EQ(Read(files[0]), kKeyFrame);
    EXPECT_EQ(Read(files[1]), kKeyFrame);
}

TEST_F(FrameRecorderTest, RemembersItsSettings) {
    Card().Set("recording", "0");
    EXPECT_TRUE(fs::exists(Settings() / "frame-recorder.ini"));
    Load();
    ASSERT_EQ(services->UiPanels().size(), 1u);
    EXPECT_EQ(Card().Get("recording"), "0");
    EXPECT_EQ(Card().Get("folder"), Utf8Of(Videos()));
}

TEST_F(FrameRecorderTest, SaysWhenItCantWriteToTheFolder) {
    const fs::path blocked = root / "blocked";
    std::ofstream(blocked) << "a file, not a folder";
    Card().Set("folder", Utf8Of(blocked / "videos"));
    StartOutput();
    Send(kKeyFrame, true);
    EXPECT_EQ(Card().Get("tone"), "danger");
    EXPECT_NE(Card().Get("status").find("Can't write to"), std::string::npos);
    EXPECT_TRUE(Logged("can't write the recording"));
    EXPECT_EQ(Card().Get("file-open"), "0");
}

}  // namespace
}  // namespace gazeora
