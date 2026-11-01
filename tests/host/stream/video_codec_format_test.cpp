// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/video_codec_format.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "gazeora/host/encode/built_in_video_codecs.h"
#include "gazeora/host/encode/bundled_video_codecs.h"
#include "gazeora/host/encode/media_foundation_encoder_provider.h"
#include "gazeora/host/stream/frame_packet.h"
#include "gazeora/host/stream/ts_muxer.h"
#include "gazeora/host/stream/video_pipeline.h"

namespace gazeora {
namespace {

// The PMT is the second TS packet; its section starts after the pointer field.
constexpr size_t kPmtSection = TsMuxer::kPacketSize + 5;
constexpr size_t kPmtVersionByte = kPmtSection + 5;
constexpr size_t kPmtStreamType = kPmtSection + 12;

const VideoCodecFormat& H264() { return BundledVideoCodecs::FormatOf(VideoCodec::H264); }
const VideoCodecFormat& H265() { return BundledVideoCodecs::FormatOf(VideoCodec::H265); }

uint8_t PmtVersion(const std::string& ts) { return (uint8_t(ts[kPmtVersionByte]) >> 1) & 0x1F; }

TEST(VideoCodecFormatTest, BuiltInFormatsDescribeTheirCodec) {
    EXPECT_EQ(H264().Codec(), VideoCodec::H264);
    EXPECT_EQ(&H264().Rtp(), &RtpPayloadFormat::H264());
    EXPECT_EQ(H264().TsStreamType(), 0x1B);
    EXPECT_EQ(H264().FrameFlags(), 0);
    EXPECT_EQ(H264().SeiHeader(), "\x06");

    EXPECT_EQ(H265().Codec(), VideoCodec::H265);
    EXPECT_EQ(&H265().Rtp(), &RtpPayloadFormat::H265());
    EXPECT_EQ(H265().TsStreamType(), 0x24);
    EXPECT_EQ(H265().FrameFlags(), kFrameFlagH265);
    EXPECT_EQ(H265().SeiHeader(), std::string("\x4E\x01", 2));
}

TEST(VideoCodecFormatTest, TellsAccessUnitDelimiters) {
    EXPECT_TRUE(H264().IsAccessUnitDelimiter("\x09\xF0"));
    EXPECT_FALSE(H264().IsAccessUnitDelimiter("\x65"));
    EXPECT_FALSE(H264().IsAccessUnitDelimiter(""));
    EXPECT_TRUE(H265().IsAccessUnitDelimiter(std::string("\x46\x01", 2)));
    EXPECT_FALSE(H265().IsAccessUnitDelimiter(std::string("\x26\x01", 2)));
    // Each delimiter it adds is one it recognizes.
    EXPECT_TRUE(H264().IsAccessUnitDelimiter(H264().AccessUnitDelimiter().substr(4)));
    EXPECT_TRUE(H265().IsAccessUnitDelimiter(H265().AccessUnitDelimiter().substr(4)));
}

TEST(VideoCodecFormatTest, OnlyBuiltInAndBundledCodecsHaveAFormat) {
    EXPECT_THROW(BundledVideoCodecs::FormatOf(VideoCodec("av1")), std::invalid_argument);
    const auto catalog = BundledVideoCodecs::Catalog();
    for (const VideoCodecInfo& info : catalog->All()) {
        ASSERT_NE(info.format, nullptr) << info.id.Id();
        EXPECT_EQ(info.format->Codec(), info.id);
    }
}

TEST(VideoCodecFormatTest, PmtVersionChangesWithTheCodec) {
    TsMuxer muxer;
    const std::string h264 = std::string("\0\0\0\1\x65", 5) + std::string(100, 'i');
    const std::string h265 = std::string("\0\0\0\1\x26\x01", 6) + std::string(100, 'k');
    std::string first, same, switched, back;
    muxer.Mux(H264(), h264, 0, true, first);
    muxer.Mux(H264(), h264, 3000, true, same);
    muxer.Mux(H265(), h265, 6000, true, switched);
    muxer.Mux(H264(), h264, 9000, true, back);
    EXPECT_EQ(PmtVersion(first), 0);
    EXPECT_EQ(PmtVersion(same), 0);
    EXPECT_EQ(PmtVersion(switched), 1);
    EXPECT_EQ(uint8_t(switched[kPmtStreamType]), 0x24);
    EXPECT_EQ(PmtVersion(back), 2);
}

TEST(VideoCodecFormatTest, PipelineNeedsH264sFormat) {
    EXPECT_THROW(
        VideoPipeline(BuiltInVideoEncoders::Factory(), std::make_shared<VideoCodecCatalog>()),
        std::invalid_argument);
}

TEST(VideoCodecFormatTest, PipelineRefusesACodecWithoutAFormat) {
    auto codecs = std::make_shared<VideoCodecCatalog>();
    BuiltInVideoCodecs::RegisterInto(*codecs);
    codecs->Register({VideoCodec("av1"), "AV1", "AV1"});  // no format
    VideoPipeline pipeline([] { return MediaFoundationEncoderProvider().Create(); }, codecs);
    VideoPipeline::Config config;
    config.codec = VideoCodec("av1");
    bool reopened = false;
    std::wstring error;
    EXPECT_FALSE(pipeline.Prepare(config, 64, 64, reopened, error));
    EXPECT_EQ(error, L"no stream format for the codec av1");
}

}  // namespace
}  // namespace gazeora
