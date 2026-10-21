#include "gazeora/hook/core/pixel_convert.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

namespace gazeora::hook {
namespace {

// Converts `src` as a single row.
std::vector<uint8_t> Convert(PixelLayout layout, const std::vector<uint8_t>& src) {
    uint32_t width = uint32_t(src.size() / SourceBytesPerPixel(layout));
    std::vector<uint8_t> dst(size_t(width) * 4, 0xCD);
    ConvertRow(layout, src.data(), dst.data(), width);
    return dst;
}

uint32_t Word(const std::vector<uint8_t>& bytes, size_t pixel) {
    uint32_t v;
    memcpy(&v, bytes.data() + pixel * 4, 4);
    return v;
}

std::vector<uint8_t> Bytes(std::initializer_list<uint32_t> words) {
    std::vector<uint8_t> bytes(words.size() * 4);
    memcpy(bytes.data(), words.begin(), bytes.size());
    return bytes;
}

TEST(PixelConvert, OutputFormat) {
    EXPECT_EQ(OutputFormat(PixelLayout::BGRA8), DXGI_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(OutputFormat(PixelLayout::BGRX8), DXGI_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(OutputFormat(PixelLayout::RGBA8), DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(OutputFormat(PixelLayout::RGBX8), DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(OutputFormat(PixelLayout::RGB10A2), DXGI_FORMAT_R10G10B10A2_UNORM);
    EXPECT_EQ(OutputFormat(PixelLayout::BGR10A2), DXGI_FORMAT_R10G10B10A2_UNORM);
    EXPECT_EQ(OutputFormat(PixelLayout::B5G6R5), DXGI_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(OutputFormat(PixelLayout::B5G5R5X1), DXGI_FORMAT_B8G8R8A8_UNORM);
}

TEST(PixelConvert, SourceBytesPerPixel) {
    EXPECT_EQ(SourceBytesPerPixel(PixelLayout::BGRA8), 4u);
    EXPECT_EQ(SourceBytesPerPixel(PixelLayout::RGBX8), 4u);
    EXPECT_EQ(SourceBytesPerPixel(PixelLayout::BGR10A2), 4u);
    EXPECT_EQ(SourceBytesPerPixel(PixelLayout::B5G6R5), 2u);
    EXPECT_EQ(SourceBytesPerPixel(PixelLayout::B5G5R5X1), 2u);
}

TEST(PixelConvert, NeedsConversion) {
    EXPECT_FALSE(NeedsConversion(PixelLayout::BGRA8));
    EXPECT_FALSE(NeedsConversion(PixelLayout::RGBA8));
    EXPECT_FALSE(NeedsConversion(PixelLayout::RGB10A2));
    EXPECT_TRUE(NeedsConversion(PixelLayout::BGRX8));
    EXPECT_TRUE(NeedsConversion(PixelLayout::RGBX8));
    EXPECT_TRUE(NeedsConversion(PixelLayout::BGR10A2));
    EXPECT_TRUE(NeedsConversion(PixelLayout::B5G6R5));
    EXPECT_TRUE(NeedsConversion(PixelLayout::B5G5R5X1));
}

TEST(PixelConvert, PassThroughLayoutsCopyUnchanged) {
    std::vector<uint8_t> src = {1, 2, 3, 4, 5, 6, 7, 8};
    EXPECT_EQ(Convert(PixelLayout::BGRA8, src), src);
    EXPECT_EQ(Convert(PixelLayout::RGBA8, src), src);
    EXPECT_EQ(Convert(PixelLayout::RGB10A2, src), src);
}

TEST(PixelConvert, PaddedLayoutsForceAlphaOpaque) {
    std::vector<uint8_t> src = {1, 2, 3, 0, 5, 6, 7, 99};
    std::vector<uint8_t> expected = {1, 2, 3, 255, 5, 6, 7, 255};
    EXPECT_EQ(Convert(PixelLayout::BGRX8, src), expected);
    EXPECT_EQ(Convert(PixelLayout::RGBX8, src), expected);
}

TEST(PixelConvert, Bgr10A2SwapsRedAndBlue) {
    // Blue 0x3FF in the low bits, green 0x155, red 0x001, alpha 2.
    uint32_t in = (2u << 30) | (0x001u << 20) | (0x155u << 10) | 0x3FFu;
    uint32_t out = (2u << 30) | (0x3FFu << 20) | (0x155u << 10) | 0x001u;
    std::vector<uint8_t> dst = Convert(PixelLayout::BGR10A2, Bytes({in, 0}));
    EXPECT_EQ(Word(dst, 0), out);
    EXPECT_EQ(Word(dst, 1), 0u);
}

TEST(PixelConvert, B5G6R5ExpandsToFullRange) {
    // White, black, pure blue, pure green, pure red.
    std::vector<uint8_t> src = {0xFF, 0xFF, 0x00, 0x00, 0x1F, 0x00, 0xE0, 0x07, 0x00, 0xF8};
    std::vector<uint8_t> expected = {
        255, 255, 255, 255,  // white
        0,   0,   0,   255,  // black
        255, 0,   0,   255,  // blue
        0,   255, 0,   255,  // green
        0,   0,   255, 255,  // red
    };
    EXPECT_EQ(Convert(PixelLayout::B5G6R5, src), expected);
}

TEST(PixelConvert, B5G6R5ReplicatesHighBitsIntoLowBits) {
    // Blue 0b10000 -> 0b10000100, green 0b100000 -> 0b10000010, red 0b00001 -> 0b00001000.
    uint16_t v = uint16_t((0b00001 << 11) | (0b100000 << 5) | 0b10000);
    std::vector<uint8_t> src = {uint8_t(v), uint8_t(v >> 8)};
    std::vector<uint8_t> expected = {0b10000100, 0b10000010, 0b00001000, 255};
    EXPECT_EQ(Convert(PixelLayout::B5G6R5, src), expected);
}

TEST(PixelConvert, B5G5R5X1IgnoresTopBit) {
    // White with and without the X/A bit, then pure red, green and blue.
    std::vector<uint8_t> src = {0xFF, 0x7F, 0xFF, 0xFF, 0x00, 0x7C, 0xE0, 0x03, 0x1F, 0x00};
    std::vector<uint8_t> expected = {
        255, 255, 255, 255,  // white, X clear
        255, 255, 255, 255,  // white, X set
        0,   0,   255, 255,  // red
        0,   255, 0,   255,  // green
        255, 0,   0,   255,  // blue
    };
    EXPECT_EQ(Convert(PixelLayout::B5G5R5X1, src), expected);
}

// The 16-bit layouts convert through precomputed tables; check every value against the
// bit-replication formula they're built from.
TEST(PixelConvert, SixteenBitLayoutsMatchTheFormulaForEveryValue) {
    auto expand = [](uint32_t v, int bits) {
        return uint8_t((v << (8 - bits)) | (v >> (2 * bits - 8)));
    };
    std::vector<uint8_t> src(65536 * 2);
    for (uint32_t v = 0; v < 65536; ++v) {
        src[2 * v] = uint8_t(v);
        src[2 * v + 1] = uint8_t(v >> 8);
    }
    std::vector<uint8_t> rgb565 = Convert(PixelLayout::B5G6R5, src);
    std::vector<uint8_t> rgb555 = Convert(PixelLayout::B5G5R5X1, src);
    for (uint32_t v = 0; v < 65536; ++v) {
        const uint8_t* a = &rgb565[4 * v];
        ASSERT_TRUE(a[0] == expand(v & 0x1F, 5) && a[1] == expand((v >> 5) & 0x3F, 6) &&
                    a[2] == expand(v >> 11, 5) && a[3] == 255)
            << "B5G6R5 value " << v;
        const uint8_t* b = &rgb555[4 * v];
        ASSERT_TRUE(b[0] == expand(v & 0x1F, 5) && b[1] == expand((v >> 5) & 0x1F, 5) &&
                    b[2] == expand((v >> 10) & 0x1F, 5) && b[3] == 255)
            << "B5G5R5X1 value " << v;
    }
}

TEST(PixelConvert, ZeroWidthWritesNothing) {
    uint8_t src[4] = {1, 2, 3, 4};
    uint8_t dst[4] = {9, 9, 9, 9};
    for (PixelLayout layout : {PixelLayout::BGRA8, PixelLayout::BGRX8, PixelLayout::BGR10A2,
                               PixelLayout::B5G6R5, PixelLayout::B5G5R5X1}) {
        ConvertRow(layout, src, dst, 0);
    }
    EXPECT_EQ(dst[0], 9);
    EXPECT_EQ(dst[3], 9);
}

TEST(PixelConvert, LayoutForD3DFormat) {
    struct Case {
        uint32_t d3dFormat;
        PixelLayout layout;
    };
    // D3DFMT_* values from d3d9types.h.
    const Case cases[] = {
        {21, PixelLayout::BGRA8},     // A8R8G8B8
        {22, PixelLayout::BGRX8},     // X8R8G8B8
        {23, PixelLayout::B5G6R5},    // R5G6B5
        {24, PixelLayout::B5G5R5X1},  // X1R5G5B5
        {25, PixelLayout::B5G5R5X1},  // A1R5G5B5
        {31, PixelLayout::RGB10A2},   // A2B10G10R10
        {32, PixelLayout::RGBA8},     // A8B8G8R8
        {33, PixelLayout::RGBX8},     // X8B8G8R8
        {35, PixelLayout::BGR10A2},   // A2R10G10B10
    };
    for (const Case& c : cases) {
        PixelLayout layout = PixelLayout::BGRA8;
        EXPECT_TRUE(LayoutForD3DFormat(c.d3dFormat, layout)) << c.d3dFormat;
        EXPECT_EQ(layout, c.layout) << c.d3dFormat;
    }
}

TEST(PixelConvert, LayoutForD3DFormatRejectsUnsupported) {
    // Unknown, R8G8B8 (24-bit), A4R4G4B4, A16B16G16R16F.
    for (uint32_t format : {0u, 20u, 26u, 113u}) {
        PixelLayout layout = PixelLayout::RGBX8;
        EXPECT_FALSE(LayoutForD3DFormat(format, layout)) << format;
        EXPECT_EQ(layout, PixelLayout::RGBX8) << "layout must be untouched on failure";
    }
}

}  // namespace
}  // namespace gazeora::hook
