#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/stream/video_pipeline.h"

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
    };
    explicit FakeEncoder(Calls& calls) : m_calls(calls) {}

    bool Open(const Settings& settings, std::wstring&) override {
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
    bool Encode(const uint8_t*, int64_t time, std::vector<Frame>& out, std::wstring&) override {
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

private:
    Calls& m_calls;
    Settings m_settings;
    std::wstring m_name = L"Fake";
    bool m_open = false;
    bool m_nextIsKey = true;
};

class VideoPipelineTest : public ::testing::Test {
protected:
    VideoPipelineTest()
        : pipeline([this] { return std::make_unique<FakeEncoder>(calls); }) {
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
            [&](const std::vector<std::string>& packets, bool key) {
                sent.emplace_back(packets.size(), key);
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

    auto second = EncodeOnce();
    ASSERT_EQ(second.size(), 1u);
    EXPECT_FALSE(second[0].second);
    EXPECT_FALSE(lastFmtpChanged);
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

}  // namespace
}  // namespace gazeora
