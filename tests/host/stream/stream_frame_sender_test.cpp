#include "gazeora/host/stream/stream_frame_sender.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "gazeora/common/text/utf8.h"
#include "support/d3d_test_util.h"

using Microsoft::WRL::ComPtr;

namespace gazeora {
namespace {

// Records what reaches the stream.
class RecordingSink final : public LiveFrameSink {
public:
    bool WantsFrames() const override { return true; }
    bool WantsTextures() override { return textures; }
    void PushFrame(uint32_t width, uint32_t height, std::vector<uint8_t>&, bool) override {
        ++frames;
        lastWidth = width;
        lastHeight = height;
    }
    void PushTexture(uint32_t width, uint32_t height, ComPtr<ID3D11Texture2D> nv12, bool) override {
        ++texturesPushed;
        lastWidth = width;
        lastHeight = height;
        lastTexture = nv12;
    }
    void RepeatFrame() override { ++repeats; }

    bool textures = true;
    int frames = 0;
    int texturesPushed = 0;
    int repeats = 0;
    uint32_t lastWidth = 0;
    uint32_t lastHeight = 0;
    ComPtr<ID3D11Texture2D> lastTexture;
};

class StreamFrameSenderTest : public ::testing::Test {
protected:
    void SetUp() override {
        warp = test::CreateWarpDevice();
        ASSERT_TRUE(warp.device) << "WARP D3D11.4 device unavailable";
        frame = test::CreateTexture(warp.device.Get(), 64, 32, DXGI_FORMAT_B8G8R8A8_UNORM,
                                    std::vector<uint32_t>(64 * 32, 0xFF336699));
        ASSERT_TRUE(frame);
    }

    bool Send() {
        std::wstring error;
        bool sent = sender.Send(warp.device.Get(), frame.Get(), nullptr, error);
        EXPECT_TRUE(sent) << Utf8::FromWide(error);
        return sent;
    }

    test::WarpDevice warp;
    ComPtr<ID3D11Texture2D> frame;
    RecordingSink sink;
    StreamFrameSender sender{sink};
};

TEST_F(StreamFrameSenderTest, SendsFramesAsTexturesWhenTheStreamTakesThem) {
    sender.FrameCaptured();
    Send();
    EXPECT_EQ(sink.texturesPushed, 1);
    EXPECT_EQ(sink.frames, 0);
    EXPECT_EQ(sink.lastWidth, 64u);
    EXPECT_EQ(sink.lastHeight, 32u);
    ASSERT_TRUE(sink.lastTexture);
    D3D11_TEXTURE2D_DESC desc;
    sink.lastTexture->GetDesc(&desc);
    EXPECT_EQ(desc.Format, DXGI_FORMAT_NV12);
}

TEST_F(StreamFrameSenderTest, ReadsFramesBackWhenTheStreamWantsPixels) {
    sink.textures = false;
    sender.FrameCaptured();
    Send();
    EXPECT_EQ(sink.frames, 1);
    EXPECT_EQ(sink.texturesPushed, 0);
}

TEST_F(StreamFrameSenderTest, RepeatsAFrameItSentAlready) {
    sender.FrameCaptured();
    Send();
    Send();
    Send();
    EXPECT_EQ(sink.texturesPushed, 1);
    EXPECT_EQ(sink.repeats, 2);
    sender.FrameCaptured();
    Send();
    EXPECT_EQ(sink.texturesPushed, 2) << "a new frame goes out as one";
}

TEST_F(StreamFrameSenderTest, SendsTheFrameAgainForANewLayout) {
    sender.FrameCaptured();
    Send();
    VrLayout vr;
    vr.enabled = true;
    vr.lensOnPhone = false;
    sender.SetVrLayout(vr);
    Send();
    EXPECT_EQ(sink.texturesPushed, 2);
    EXPECT_EQ(sink.repeats, 0);
}

}  // namespace
}  // namespace gazeora
