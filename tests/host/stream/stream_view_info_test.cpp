#include "gazeora/host/stream/stream_view_info.h"

#include <gtest/gtest.h>

#include <string>

namespace gazeora {
namespace {

VrLayout PhoneVr() {
    VrLayout vr;
    vr.enabled = true;
    vr.lensOnPhone = true;
    return vr;
}

TEST(StreamViewInfoTest, SaysNothingUnlessThePhoneDoesTheLenses) {
    EXPECT_EQ(ViewInfoText(VrLayout(), false), "");
    VrLayout pc = PhoneVr();
    pc.lensOnPhone = false;  // the frames are warped already
    EXPECT_EQ(ViewInfoText(pc, true), "");
}

TEST(StreamViewInfoTest, CarriesTheLensSettingsAndWhetherItsAPair) {
    VrLayout vr = PhoneVr();
    vr.k1 = 0.25f;
    vr.k2 = 0.5f;
    vr.imageScale = 0.8f;
    vr.lensSeparation = 0.52f;
    vr.chromaCorrection = 0.01f;
    EXPECT_EQ(ViewInfoText(vr, true),
              "vr=1 pair=1 k1=0.250 k2=0.500 scale=0.800 sep=0.520 chroma=0.010");
    EXPECT_EQ(ViewInfoText(vr, false).substr(0, 11), "vr=1 pair=0");
}

TEST(StreamViewInfoTest, IsAnUnregisteredUserDataSei) {
    std::string text = "vr=1 pair=0";
    std::string h264 = ViewInfoNal(VideoCodec::H264, text);
    EXPECT_EQ(h264[0], '\x06');  // SEI
    EXPECT_EQ(h264[1], '\x05');  // user data unregistered
    EXPECT_EQ(uint8_t(h264[2]), 16 + text.size());
    EXPECT_EQ(h264.substr(3, 16), "GazeoraViewInfo1");
    EXPECT_EQ(h264.substr(19, text.size()), text);
    EXPECT_EQ(h264.back(), '\x80');

    std::string h265 = ViewInfoNal(VideoCodec::H265, text);
    EXPECT_EQ(h265.substr(0, 2), std::string("\x4E\x01", 2));  // prefix SEI, layer 0, tid 0
    EXPECT_EQ(h265.substr(2), h264.substr(1));
}

TEST(StreamViewInfoTest, LongTextsTakeSizeBytesOf255) {
    std::string text(300, 'x');
    std::string nal = ViewInfoNal(VideoCodec::H264, text);
    EXPECT_EQ(uint8_t(nal[2]), 0xFF);
    EXPECT_EQ(uint8_t(nal[3]), 16 + 300 - 255);
}

TEST(StreamViewInfoTest, TellsPicturesFromTheRest) {
    EXPECT_TRUE(IsPictureNal(VideoCodec::H264, "\x65"));   // IDR
    EXPECT_TRUE(IsPictureNal(VideoCodec::H264, "\x41"));   // P slice
    EXPECT_FALSE(IsPictureNal(VideoCodec::H264, "\x67"));  // SPS
    EXPECT_FALSE(IsPictureNal(VideoCodec::H264, "\x06"));  // SEI
    EXPECT_TRUE(IsPictureNal(VideoCodec::H265, std::string("\x26\x01", 2)));   // IDR_W_RADL
    EXPECT_TRUE(IsPictureNal(VideoCodec::H265, std::string("\x02\x01", 2)));   // TRAIL_R
    EXPECT_FALSE(IsPictureNal(VideoCodec::H265, std::string("\x40\x01", 2)));  // VPS
    EXPECT_FALSE(IsPictureNal(VideoCodec::H265, std::string("\x4E\x01", 2)));  // SEI
}

}  // namespace
}  // namespace gazeora
