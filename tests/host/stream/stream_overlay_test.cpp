#include "gazeora/host/stream/stream_overlay.h"

#include <gtest/gtest.h>

namespace gazeora {
namespace {

OverlayStats Stats() {
    OverlayStats stats;
    stats.protocol = "RTSP over TCP";
    stats.viewers = 1;
    stats.codec = "H.265";
    stats.width = 1920;
    stats.height = 1080;
    stats.frameRate = 30;
    stats.bitrate = 5600000;
    stats.loss = 0.012f;
    stats.encoder = L"NVIDIA HEVC Encoder MFT";
    stats.time.wHour = 9;
    stats.time.wMinute = 5;
    stats.time.wSecond = 7;
    return stats;
}

TEST(StreamOverlayTest, LinesHoldThePickedDetails) {
    OverlaySettings settings;
    std::vector<std::wstring> lines = OverlayLines(settings, Stats());
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], L"RTSP over TCP  ·  1 viewer");
    EXPECT_EQ(lines[1], L"H.265  ·  1920×1080  ·  30 fps");
    EXPECT_EQ(lines[2], L"5.6 Mbit/s  ·  1.2% lost");

    settings.protocol = settings.viewers = settings.bitrate = settings.loss = false;
    settings.codec = settings.resolution = settings.frameRate = false;
    settings.encoder = settings.clock = true;
    settings.text = L"Living room PC";
    lines = OverlayLines(settings, Stats());
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], L"NVIDIA HEVC Encoder MFT");
    EXPECT_EQ(lines[1], L"09:05:07");
    EXPECT_EQ(lines[2], L"Living room PC");

    settings.encoder = settings.clock = false;
    settings.text.clear();
    EXPECT_TRUE(OverlayLines(settings, Stats()).empty());
}

Picture Grey(uint32_t width, uint32_t height) {
    Picture picture;
    picture.width = width;
    picture.height = height;
    picture.bgra.assign(size_t(width) * height * 4, 0x80);
    return picture;
}

bool Untouched(const Picture& picture, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    for (uint32_t y = y0; y < y1; ++y) {
        for (uint32_t x = x0; x < x1; ++x) {
            const uint8_t* p = &picture.bgra[(size_t(y) * picture.width + x) * 4];
            if (p[0] != 0x80 || p[1] != 0x80 || p[2] != 0x80) {
                return false;
            }
        }
    }
    return true;
}

TEST(StreamOverlayTest, DrawsAPanelInTheCornerPicked) {
    OverlaySettings settings;
    settings.corner = OverlaySettings::Corner::BottomRight;
    OverlayRenderer renderer;
    Picture picture = Grey(640, 360);
    renderer.Draw({L"RTSP over TCP", L"30 fps"}, settings, OverlayLayout::Flat, picture);
    // The bottom right quarter has the panel (darker, with white text); the rest is as it was.
    EXPECT_FALSE(Untouched(picture, 320, 180, 640, 360));
    EXPECT_TRUE(Untouched(picture, 0, 0, 320, 180));
    EXPECT_TRUE(Untouched(picture, 0, 180, 200, 360));
    bool white = false, dark = false;
    for (size_t i = 0; i < picture.bgra.size(); i += 4) {
        white |= picture.bgra[i] > 0xF0;
        dark |= picture.bgra[i] < 0x50;
    }
    EXPECT_TRUE(white);
    EXPECT_TRUE(dark);
}

TEST(StreamOverlayTest, DrawsIntoEachEyeInVr) {
    OverlaySettings settings;  // top left
    OverlayRenderer renderer;
    Picture picture = Grey(1280, 360);
    renderer.Draw({L"H.264"}, settings, OverlayLayout::VrEyes, picture);
    // Each eye's top left, inset from the lens edges: not the picture's own corner.
    EXPECT_TRUE(Untouched(picture, 0, 0, 100, 60));
    EXPECT_FALSE(Untouched(picture, 128, 72, 640, 360));
    EXPECT_FALSE(Untouched(picture, 768, 72, 1280, 360));
}

// How many columns of x0..x1 the overlay drew on.
uint32_t DrawnWidth(const Picture& picture, uint32_t x0, uint32_t x1) {
    uint32_t drawn = 0;
    for (uint32_t x = x0; x < x1; ++x) {
        drawn += Untouched(picture, x, 0, x + 1, picture.height) ? 0 : 1;
    }
    return drawn;
}

TEST(StreamOverlayTest, DrawsIntoBothViewsOfAPairSqueezedLikeThem) {
    OverlaySettings settings;  // top left
    OverlayRenderer renderer;
    Picture flat = Grey(1280, 360);
    renderer.Draw({L"H.264  ·  1920×1080"}, settings, OverlayLayout::Flat, flat);
    Picture pair = Grey(1280, 360);
    renderer.Draw({L"H.264  ·  1920×1080"}, settings, OverlayLayout::Pair, pair);
    uint32_t full = DrawnWidth(flat, 0, 1280);
    uint32_t left = DrawnWidth(pair, 0, 640), right = DrawnWidth(pair, 640, 1280);
    EXPECT_EQ(left, right) << "the same in both views";
    EXPECT_NEAR(double(left), full / 2.0, 1.0) << "the phone stretches each view to twice";
    // At the same place in each view, so it sits at the screen's depth.
    EXPECT_EQ(Untouched(pair, 0, 0, 20, 360), Untouched(pair, 640, 0, 660, 360));
}

}  // namespace
}  // namespace gazeora
