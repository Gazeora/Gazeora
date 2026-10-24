#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/stream/video_pipeline.h"

#include "gazeora/common/text/utf8.h"
#include "gazeora/host/encode/gpu_nv12_converter.h"
#include "support/d3d_test_util.h"

namespace gazeora {
namespace {

// Emits an H.264 access unit per frame: SPS + PPS + IDR for the first and for requested key
// frames, a P slice otherwise. Records what the pipeline asked of it.
class FakeEncoder final : public VideoEncoder {
public:
    struct Calls {
        int opens = 0;
        uint32_t bitrate = 0;
        int keyFrameRequests = 0;
        bool takesTextures = false;  // Open with a device fails otherwise
        int memoryFrames = 0;
        int textureFrames = 0;
        uint8_t firstLuma = 0;  // of the last frame from memory
    };
    explicit FakeEncoder(Calls& calls) : m_calls(calls) {}

    bool Open(const Settings& settings, std::wstring& error) override {
        if (settings.device && !m_calls.takesTextures) {
            error = L"no textures";
            return false;
        }
        m_settings = settings;
        m_open = true;
        m_nextIsKey = true;
        ++m_calls.opens;
        m_calls.bitrate = settings.bitrate;
        return true;
    }
    void Close() override { m_open = false; }
    bool IsOpen() const override { return m_open; }
    const Settings& Current() const override { return m_settings; }
    const std::wstring& Name() const override { return m_name; }
    bool IsHardware() const override { return false; }
    void SetBitrate(uint32_t bitrate) override { m_calls.bitrate = bitrate; }
    void RequestKeyFrame() override {
        m_nextIsKey = true;
        ++m_calls.keyFrameRequests;
    }
    bool Encode(const uint8_t* nv12, int64_t time, std::vector<Frame>& out,
                std::wstring&) override {
        ++m_calls.memoryFrames;
        m_calls.firstLuma = nv12[0];
        return Emit(time, out);
    }
    bool EncodeTexture(ID3D11Texture2D*, int64_t time, std::vector<Frame>& out,
                       std::wstring&) override {
        ++m_calls.textureFrames;
        return Emit(time, out);
    }

private:
    bool Emit(int64_t time, std::vector<Frame>& out) {
        static const std::string kStart("\0\0\0\1", 4);
        Frame frame;
        frame.time = time;
        frame.keyFrame = m_nextIsKey;
        frame.annexB = m_nextIsKey ? kStart + "\x67\x42\xC0\x28" + kStart + "\x68\xCE" + kStart +
                                         "\x65" + std::string(3000, 'i')
                                   : kStart + "\x41" + std::string(100, 'p');
        // Parameter sets only with the first frame, like encoders that send them once.
        m_nextIsKey = false;
        out.push_back(frame);
        return true;
    }

    Calls& m_calls;
    Settings m_settings;
    std::wstring m_name = L"Fake";
    bool m_open = false;
    bool m_nextIsKey = true;
};

class VideoPipelineTest : public ::testing::Test {
protected:
    VideoPipelineTest() : pipeline([this] { return std::make_unique<FakeEncoder>(calls); }) {
        config.bitrate = 8000000;
        config.ssrc = 0x1234;
        picture.width = 64;
        picture.height = 32;
        picture.bgra.assign(64 * 32 * 4, 0x80);
    }

    // Encodes `picture` and returns the packet counts per frame, with key frames marked.
    std::vector<std::pair<size_t, bool>> EncodeOnce() {
        std::vector<std::pair<size_t, bool>> sent;
        bool fmtpChanged = false;
        std::wstring error;
        EXPECT_TRUE(pipeline.Encode(
            picture,
            [&](const VideoPipeline::EncodedFrame& frame) {
                sent.emplace_back(frame.packets.size(), frame.keyFrame);
                lastAnnexB = frame.annexB;
            },
            fmtpChanged, error));
        lastFmtpChanged = fmtpChanged;
        return sent;
    }

    FakeEncoder::Calls calls;
    VideoPipeline pipeline;
    VideoPipeline::Config config;
    Picture picture;
    bool lastFmtpChanged = false;
    std::string lastAnnexB;  // the last frame's access unit
};

TEST_F(VideoPipelineTest, OpensOnceAndOnlyReopensForANewSize) {
    bool reopened = false;
    std::wstring error;
    ASSERT_TRUE(pipeline.Prepare(config, 64, 32, reopened, error));
    EXPECT_TRUE(reopened);
    ASSERT_TRUE(pipeline.Prepare(config, 64, 32, reopened, error));
    EXPECT_FALSE(reopened);
    ASSERT_TRUE(pipeline.Prepare(config, 128, 64, reopened, error));
    EXPECT_TRUE(reopened);
    EXPECT_EQ(calls.opens, 2);
}

TEST_F(VideoPipelineTest, ReopensForANewFrameRate) {
    bool reopened = false;
    std::wstring error;
    ASSERT_TRUE(pipeline.Prepare(config, 64, 32, reopened, error));
    config.fps = 60;
    config.keyInterval = 60;
    ASSERT_TRUE(pipeline.Prepare(config, 64, 32, reopened, error));
    EXPECT_TRUE(reopened);
    ASSERT_TRUE(pipeline.Prepare(config, 64, 32, reopened, error));
    EXPECT_FALSE(reopened);
    EXPECT_EQ(calls.opens, 2);
}

TEST_F(VideoPipelineTest, AppliesNewBitratesWithoutReopening) {
    bool reopened = false;
    std::wstring error;
    ASSERT_TRUE(pipeline.Prepare(config, 64, 32, reopened, error));
    config.bitrate = 3000000;
    ASSERT_TRUE(pipeline.Prepare(config, 64, 32, reopened, error));
    EXPECT_FALSE(reopened);
    EXPECT_EQ(calls.bitrate, 3000000u);
    EXPECT_EQ(calls.opens, 1);
}

TEST_F(VideoPipelineTest, MarksKeyFramesAndPublishesParameterSets) {
    bool reopened = false;
    std::wstring error;
    ASSERT_TRUE(pipeline.Prepare(config, 64, 32, reopened, error));

    auto first = EncodeOnce();
    ASSERT_EQ(first.size(), 1u);
    EXPECT_TRUE(first[0].second);
    EXPECT_GT(first[0].first, 2u) << "the 3 KB IDR slice is fragmented";
    EXPECT_TRUE(lastFmtpChanged);
    EXPECT_NE(pipeline.Fmtp().find("sprop-parameter-sets="), std::string::npos);
    // The access unit for the protocols without RTP: SPS, PPS and the slice, start codes in
    // front of each.
    ASSERT_GT(lastAnnexB.size(), 12u);
    EXPECT_EQ(lastAnnexB.substr(0, 5), std::string("\0\0\0\1\x67", 5));
    EXPECT_NE(lastAnnexB.find(std::string("\0\0\0\1\x68", 5)), std::string::npos);
    EXPECT_NE(lastAnnexB.find(std::string("\0\0\0\1\x65", 5)), std::string::npos);

    auto second = EncodeOnce();
    ASSERT_EQ(second.size(), 1u);
    EXPECT_FALSE(second[0].second);
    EXPECT_FALSE(lastFmtpChanged);
}

TEST_F(VideoPipelineTest, PutsTheViewInfoJustBeforeThePicture) {
    bool reopened = false;
    std::wstring error;
    ASSERT_TRUE(pipeline.Prepare(config, 64, 32, reopened, error));
    pipeline.SetViewInfo("vr=1 pair=1");
    const std::string sei("\0\0\0\1\x06\x05", 6);

    EncodeOnce();
    size_t pps = lastAnnexB.find(std::string("\0\0\0\1\x68", 5));
    size_t at = lastAnnexB.find(sei);
    size_t idr = lastAnnexB.find(std::string("\0\0\0\1\x65", 5));
    ASSERT_NE(at, std::string::npos);
    EXPECT_LT(pps, at) << "after the parameter sets";
    EXPECT_LT(at, idr) << "before the slice";
    EXPECT_NE(lastAnnexB.find("GazeoraViewInfo1vr=1 pair=1"), std::string::npos);

    EncodeOnce();  // every frame has it
    EXPECT_EQ(lastAnnexB.find(sei), 0u);
    EXPECT_NE(lastAnnexB.find(std::string("\0\0\0\1\x41", 5)), std::string::npos);

    pipeline.SetViewInfo("");
    EncodeOnce();
    EXPECT_EQ(lastAnnexB.find(sei), std::string::npos);
}

TEST_F(VideoPipelineTest, ForwardsKeyFrameRequests) {
    bool reopened = false;
    std::wstring error;
    ASSERT_TRUE(pipeline.Prepare(config, 64, 32, reopened, error));
    EncodeOnce();
    pipeline.RequestKeyFrame();
    auto next = EncodeOnce();
    ASSERT_EQ(next.size(), 1u);
    EXPECT_TRUE(next[0].second);
    EXPECT_EQ(calls.keyFrameRequests, 1);
}

class VideoPipelineTextureTest : public VideoPipelineTest {
protected:
    void SetUp() override {
        warp = test::CreateWarpDevice();
        ASSERT_TRUE(warp.device) << "WARP D3D11.4 device unavailable";
        // White: luma 235 in studio range.
        auto frame = test::CreateTexture(warp.device.Get(), 64, 32, DXGI_FORMAT_B8G8R8A8_UNORM,
                                         std::vector<uint32_t>(64 * 32, 0xFFFFFFFF));
        UINT width = 0, height = 0;
        std::wstring error;
        picture.nv12 = converter.Convert(warp.device.Get(), frame.Get(), width, height, error);
        ASSERT_TRUE(picture.nv12) << Utf8::FromWide(error);
        picture.bgra.clear();
    }

    bool Prepare(ID3D11Device* device, bool& reopened) {
        std::wstring error;
        return pipeline.Prepare(config, 64, 32, reopened, error, device);
    }

    test::WarpDevice warp;
    GpuNv12Converter converter;
};

TEST_F(VideoPipelineTextureTest, EncodesTexturesWithAnEncoderThatTakesThem) {
    calls.takesTextures = true;
    bool reopened = false;
    ASSERT_TRUE(Prepare(warp.device.Get(), reopened));
    EXPECT_TRUE(reopened);
    ASSERT_EQ(EncodeOnce().size(), 1u);
    EXPECT_EQ(calls.textureFrames, 1);
    EXPECT_EQ(calls.memoryFrames, 0);
}

TEST_F(VideoPipelineTextureTest, ReadsTexturesBackForAnEncoderThatTakesNone) {
    bool reopened = false;
    ASSERT_TRUE(Prepare(warp.device.Get(), reopened));
    EXPECT_TRUE(reopened);
    ASSERT_EQ(EncodeOnce().size(), 1u);
    EXPECT_EQ(calls.memoryFrames, 1);
    EXPECT_EQ(calls.textureFrames, 0);
    EXPECT_EQ(calls.firstLuma, 235);
    // The refusal is remembered: the next frame doesn't try again.
    ASSERT_TRUE(Prepare(warp.device.Get(), reopened));
    EXPECT_FALSE(reopened);
    EXPECT_EQ(calls.opens, 1);
}

TEST_F(VideoPipelineTextureTest, ReopensBetweenTexturesAndPicturesInMemory) {
    calls.takesTextures = true;
    bool reopened = false;
    ASSERT_TRUE(Prepare(warp.device.Get(), reopened));
    ASSERT_TRUE(Prepare(nullptr, reopened));
    EXPECT_TRUE(reopened) << "the idle picture comes from memory";
    ASSERT_TRUE(Prepare(warp.device.Get(), reopened));
    EXPECT_TRUE(reopened);
    EXPECT_EQ(calls.opens, 3);
}

}  // namespace
}  // namespace gazeora
