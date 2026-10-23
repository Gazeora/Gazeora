#include "gazeora/host/capture/frame_dump.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "support/d3d_test_util.h"

namespace gazeora {
namespace {

struct Bmp {
    BITMAPFILEHEADER file = {};
    BITMAPINFOHEADER info = {};
    std::vector<uint32_t> pixels;  // BGRA8
};

bool ReadBmp(const std::wstring& path, Bmp& bmp) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    bool ok = fread(&bmp.file, sizeof(bmp.file), 1, f) == 1 &&
              fread(&bmp.info, sizeof(bmp.info), 1, f) == 1;
    if (ok) {
        bmp.pixels.resize(size_t(bmp.info.biWidth) * size_t(-bmp.info.biHeight));
        ok = fread(bmp.pixels.data(), 4, bmp.pixels.size(), f) == bmp.pixels.size() &&
             fgetc(f) == EOF;
    }
    fclose(f);
    return ok;
}

class FrameDumpTest : public ::testing::Test {
protected:
    void SetUp() override {
        warp = test::CreateWarpDevice();
        ASSERT_TRUE(warp.device) << "WARP D3D11.4 device unavailable";
        wchar_t dir[MAX_PATH];
        GetTempPathW(MAX_PATH, dir);
        path = std::wstring(dir) + L"gazeora_frame_dump_test_" +
               std::to_wstring(GetCurrentProcessId()) + L".bmp";
    }

    void TearDown() override { DeleteFileW(path.c_str()); }

    // Saves a 2x2 texture of `format` holding `pixels` and reads the BMP back.
    Bmp SaveAndRead(DXGI_FORMAT format, const std::vector<uint32_t>& pixels) {
        auto texture = test::CreateTexture(warp.device.Get(), 2, 2, format, pixels);
        EXPECT_TRUE(texture);
        std::wstring error;
        EXPECT_TRUE(SaveTextureAsBmp(warp.device.Get(), texture.Get(), path, error)) << error;
        Bmp bmp;
        EXPECT_TRUE(ReadBmp(path, bmp));
        return bmp;
    }

    test::WarpDevice warp;
    std::wstring path;
};

TEST_F(FrameDumpTest, WritesTopDown32BitHeader) {
    Bmp bmp = SaveAndRead(DXGI_FORMAT_B8G8R8A8_UNORM, {0, 0, 0, 0});
    EXPECT_EQ(bmp.file.bfType, 0x4D42);
    EXPECT_EQ(bmp.file.bfOffBits, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));
    EXPECT_EQ(bmp.file.bfSize, bmp.file.bfOffBits + 2 * 2 * 4);
    EXPECT_EQ(bmp.info.biWidth, 2);
    EXPECT_EQ(bmp.info.biHeight, -2) << "negative height: top-down rows";
    EXPECT_EQ(bmp.info.biBitCount, 32);
    EXPECT_EQ(bmp.info.biCompression, DWORD(BI_RGB));
}

TEST_F(FrameDumpTest, Bgra8KeepsChannelsAndForcesAlpha) {
    // Memory order B, G, R, A; as little-endian words 0xAARRGGBB.
    Bmp bmp =
        SaveAndRead(DXGI_FORMAT_B8G8R8A8_UNORM, {0x00112233, 0x80FF0000, 0x4000FF00, 0xFF0000FF});
    EXPECT_EQ(bmp.pixels, (std::vector<uint32_t>{0xFF112233, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF}));
}

TEST_F(FrameDumpTest, Rgba8SwapsRedAndBlue) {
    // Memory order R, G, B, A; as little-endian words 0xAABBGGRR.
    Bmp bmp =
        SaveAndRead(DXGI_FORMAT_R8G8B8A8_UNORM, {0x00332211, 0xFF0000FF, 0xFF00FF00, 0xFFFF0000});
    EXPECT_EQ(bmp.pixels, (std::vector<uint32_t>{0xFF112233, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF}));
}

TEST_F(FrameDumpTest, Rgb10A2KeepsTopEightBits) {
    auto rgb10 = [](uint32_t r, uint32_t g, uint32_t b) { return (b << 20) | (g << 10) | r; };
    Bmp bmp = SaveAndRead(
        DXGI_FORMAT_R10G10B10A2_UNORM,
        {rgb10(0x3FF, 0, 0), rgb10(0, 0x3FF, 0), rgb10(0, 0, 0x3FF), rgb10(0x004, 0x200, 0x003)});
    EXPECT_EQ(bmp.pixels, (std::vector<uint32_t>{0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFF018000}));
}

// Rows are converted four pixels at a time, the rest one by one: 13 pixels wide takes both.
TEST_F(FrameDumpTest, WideRowsConvertEveryPixel) {
    constexpr UINT kWidth = 13, kHeight = 3;
    std::vector<uint32_t> pixels(kWidth * kHeight);
    uint32_t seed = 3;
    for (uint32_t& p : pixels) p = seed = seed * 1664525u + 1013904223u;
    struct Case {
        DXGI_FORMAT format;
        uint32_t (*expected)(uint32_t);
    };
    const Case cases[] = {
        {DXGI_FORMAT_B8G8R8A8_UNORM, [](uint32_t v) { return v | 0xFF000000; }},
        {DXGI_FORMAT_R8G8B8A8_UNORM,
         [](uint32_t v) {
             uint32_t r = v & 0xFF, g = (v >> 8) & 0xFF, b = (v >> 16) & 0xFF;
             return 0xFF000000 | r << 16 | g << 8 | b;
         }},
        {DXGI_FORMAT_R10G10B10A2_UNORM,
         [](uint32_t v) {
             uint32_t r = (v & 0x3FF) >> 2, g = ((v >> 10) & 0x3FF) >> 2, b = ((v >> 20) & 0x3FF) >> 2;
             return 0xFF000000 | r << 16 | g << 8 | b;
         }},
    };
    for (const Case& c : cases) {
        auto texture = test::CreateTexture(warp.device.Get(), kWidth, kHeight, c.format, pixels);
        ASSERT_TRUE(texture);
        TextureReader reader;
        UINT width = 0, height = 0;
        std::vector<uint8_t> bgra;
        std::wstring error;
        ASSERT_TRUE(reader.Read(warp.device.Get(), texture.Get(), width, height, bgra, error));
        ASSERT_EQ(bgra.size(), size_t(kWidth) * kHeight * 4);
        for (size_t i = 0; i < pixels.size(); ++i) {
            uint32_t got;
            memcpy(&got, bgra.data() + i * 4, 4);
            ASSERT_EQ(got, c.expected(pixels[i])) << "format " << c.format << ", pixel " << i;
        }
    }
}

TEST_F(FrameDumpTest, RejectsUnsupportedFormat) {
    auto texture =
        test::CreateTexture(warp.device.Get(), 2, 2, DXGI_FORMAT_R16G16_UNORM, {0, 0, 0, 0});
    ASSERT_TRUE(texture);
    std::wstring error;
    EXPECT_FALSE(SaveTextureAsBmp(warp.device.Get(), texture.Get(), path, error));
    EXPECT_EQ(error, L"BMP export does not support DXGI format 35");
    EXPECT_EQ(GetFileAttributesW(path.c_str()), INVALID_FILE_ATTRIBUTES) << "no file written";
}

TEST_F(FrameDumpTest, RejectsMissingFrame) {
    std::wstring error;
    EXPECT_FALSE(SaveTextureAsBmp(nullptr, nullptr, path, error));
    EXPECT_EQ(error, L"no frame captured yet");
    EXPECT_FALSE(SaveTextureAsBmp(warp.device.Get(), nullptr, path, error));
}

TEST_F(FrameDumpTest, ReportsUnwritablePath) {
    auto texture =
        test::CreateTexture(warp.device.Get(), 2, 2, DXGI_FORMAT_B8G8R8A8_UNORM, {0, 0, 0, 0});
    std::wstring badPath = L"Z:\\gazeora\\no\\such\\dir\\frame.bmp";
    std::wstring error;
    EXPECT_FALSE(SaveTextureAsBmp(warp.device.Get(), texture.Get(), badPath, error));
    EXPECT_EQ(error, L"cannot open " + badPath + L" for writing");
}

// Reads `texture` through a TextureReader limited to maxWidth x maxHeight.
std::vector<uint32_t> ReadScaled(ID3D11Device* device, ID3D11Texture2D* texture, UINT maxWidth,
                                 UINT maxHeight, UINT& width, UINT& height) {
    TextureReader reader;
    reader.SetMaxSize(maxWidth, maxHeight);
    std::vector<uint8_t> bgra;
    std::wstring error;
    EXPECT_TRUE(reader.Read(device, texture, width, height, bgra, error));
    std::vector<uint32_t> pixels(bgra.size() / 4);
    memcpy(pixels.data(), bgra.data(), bgra.size());
    return pixels;
}

TEST_F(FrameDumpTest, ScalesLargeFramesDownOnTheGpuKeepingAspect) {
    auto texture = test::CreateTexture(warp.device.Get(), 3840, 2160, DXGI_FORMAT_R8G8B8A8_UNORM,
                                       std::vector<uint32_t>(3840 * 2160, 0xFF336699));
    UINT width, height;
    std::vector<uint32_t> pixels =
        ReadScaled(warp.device.Get(), texture.Get(), 1920, 1080, width, height);
    EXPECT_EQ(width, 1920u);
    EXPECT_EQ(height, 1080u);
    // RGBA in, BGRA out, same color.
    EXPECT_EQ(pixels.front(), 0xFF996633u);
    EXPECT_EQ(pixels.back(), 0xFF996633u);

    auto tall = test::CreateTexture(warp.device.Get(), 1000, 3000, DXGI_FORMAT_B8G8R8A8_UNORM,
                                    std::vector<uint32_t>(1000 * 3000, 0));
    ReadScaled(warp.device.Get(), tall.Get(), 1920, 1080, width, height);
    EXPECT_EQ(width, 360u);
    EXPECT_EQ(height, 1080u);
}

TEST_F(FrameDumpTest, HalfSizeAveragesEachTwoByTwoBlock) {
    // Two 2x2 blocks: 0/100/200/40 (avg 85) on the left, all 250 on the right, in blue.
    auto texture = test::CreateTexture(warp.device.Get(), 4, 2, DXGI_FORMAT_B8G8R8A8_UNORM,
                                       {0xFF000000, 0xFF000064, 0xFF0000FA, 0xFF0000FA, 0xFF0000C8,
                                        0xFF000028, 0xFF0000FA, 0xFF0000FA});
    UINT width, height;
    std::vector<uint32_t> pixels =
        ReadScaled(warp.device.Get(), texture.Get(), 2, 1, width, height);
    ASSERT_EQ(width, 2u);
    ASSERT_EQ(height, 1u);
    EXPECT_NEAR(int(pixels[0] & 0xFF), 85, 1);
    EXPECT_EQ(pixels[1] & 0xFF, 250u);
}

TEST_F(FrameDumpTest, SrgbFramesKeepTheirValuesWhenScaled) {
    auto texture = test::CreateTexture(warp.device.Get(), 4, 4, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                                       std::vector<uint32_t>(16, 0xFF804020));
    UINT width, height;
    std::vector<uint32_t> pixels =
        ReadScaled(warp.device.Get(), texture.Get(), 2, 2, width, height);
    ASSERT_EQ(width, 2u);
    for (uint32_t pixel : pixels) {
        EXPECT_NEAR(int(pixel & 0xFF), 0x20, 1);
        EXPECT_NEAR(int(pixel >> 8 & 0xFF), 0x40, 1);
        EXPECT_NEAR(int(pixel >> 16 & 0xFF), 0x80, 1);
    }
}

TEST_F(FrameDumpTest, FramesThatFitAreReadAsIs) {
    std::vector<uint32_t> source = {0xFF112233, 0xFF445566, 0xFF778899, 0xFFAABBCC};
    auto texture = test::CreateTexture(warp.device.Get(), 2, 2, DXGI_FORMAT_B8G8R8A8_UNORM, source);
    UINT width, height;
    EXPECT_EQ(ReadScaled(warp.device.Get(), texture.Get(), 1920, 1080, width, height), source);
    EXPECT_EQ(width, 2u);
}

TEST(VrLayoutTest, FrameTakesTheScreenShapeWithinTheMaxSize) {
    VrLayout vr;
    uint32_t width, height;
    vr.screenAspect = 2.0f;
    vr.FrameSize(1920, 1080, width, height);
    EXPECT_EQ(width, 1920u);
    EXPECT_EQ(height, 960u);
    vr.screenAspect = 1.5f;  // taller than 16:9: the height limits it
    vr.FrameSize(1920, 1080, width, height);
    EXPECT_EQ(width, 1620u);
    EXPECT_EQ(height, 1080u);
    vr.screenAspect = 20.0f / 9;
    vr.FrameSize(1920, 1080, width, height);
    EXPECT_EQ(width, 1920u);
    EXPECT_EQ(height, 864u);
}

// Reads `texture` through the VR pass into a 400x200 frame: two square 200x200 eyes.
std::vector<uint32_t> ReadVr(ID3D11Device* device, ID3D11Texture2D* texture, VrLayout vr) {
    vr.enabled = true;
    vr.lensOnPhone = false;  // the PC warps
    vr.screenAspect = 2.0f;
    TextureReader reader;
    reader.SetMaxSize(400, 200);
    reader.SetVrLayout(vr);
    std::vector<uint8_t> bgra;
    UINT width = 0, height = 0;
    std::wstring error;
    EXPECT_TRUE(reader.Read(device, texture, width, height, bgra, error));
    EXPECT_EQ(width, 400u);
    EXPECT_EQ(height, 200u);
    std::vector<uint32_t> pixels(bgra.size() / 4);
    memcpy(pixels.data(), bgra.data(), bgra.size());
    return pixels;
}

uint32_t At(const std::vector<uint32_t>& pixels, int x, int y) { return pixels[y * 400 + x]; }

TEST_F(FrameDumpTest, VrWithoutDistortionShowsTheWholePictureToEachEye) {
    // Square, so it fills a square eye exactly at picture size 1: red left, blue right.
    std::vector<uint32_t> source(100 * 100);
    for (int i = 0; i < 100 * 100; ++i) source[i] = i % 100 < 50 ? 0xFFFF0000 : 0xFF0000FF;
    auto texture =
        test::CreateTexture(warp.device.Get(), 100, 100, DXGI_FORMAT_B8G8R8A8_UNORM, source);
    VrLayout vr;
    vr.k1 = vr.k2 = 0;
    vr.imageScale = 1;
    std::vector<uint32_t> frame = ReadVr(warp.device.Get(), texture.Get(), vr);
    for (int eye = 0; eye < 2; ++eye) {
        EXPECT_EQ(At(frame, eye * 200 + 50, 100), 0xFFFF0000u) << "eye " << eye;
        EXPECT_EQ(At(frame, eye * 200 + 150, 100), 0xFF0000FFu) << "eye " << eye;
    }
}

TEST_F(FrameDumpTest, VrLensSeparationMovesTheEyeCenters) {
    auto texture = test::CreateTexture(warp.device.Get(), 8, 8, DXGI_FORMAT_B8G8R8A8_UNORM,
                                       std::vector<uint32_t>(64, 0xFFFFFFFF));
    VrLayout vr;
    vr.k1 = vr.k2 = 0;
    vr.imageScale = 0.2f;      // a small picture, 40 px across, around each lens center
    vr.lensSeparation = 0.6f;  // centers 240 px apart instead of 200: at x = 80 and 320
    std::vector<uint32_t> frame = ReadVr(warp.device.Get(), texture.Get(), vr);
    EXPECT_EQ(At(frame, 80, 100), 0xFFFFFFFFu);
    EXPECT_EQ(At(frame, 320, 100), 0xFFFFFFFFu);
    EXPECT_EQ(At(frame, 110, 100), 0xFF000000u);  // the middle of the left half: empty now
    EXPECT_EQ(At(frame, 290, 100), 0xFF000000u);
}

TEST_F(FrameDumpTest, VrDistortionPullsThePictureTowardTheLensCenter) {
    auto texture = test::CreateTexture(warp.device.Get(), 8, 8, DXGI_FORMAT_B8G8R8A8_UNORM,
                                       std::vector<uint32_t>(64, 0xFFFFFFFF));
    VrLayout vr;
    vr.k1 = vr.k2 = 0;
    vr.imageScale = 1;
    EXPECT_EQ(At(ReadVr(warp.device.Get(), texture.Get(), vr), 190, 100), 0xFFFFFFFFu);
    vr.k1 = 0.5f;  // barrel: the picture's edge moves in from the eye's edge
    std::vector<uint32_t> barrel = ReadVr(warp.device.Get(), texture.Get(), vr);
    EXPECT_EQ(At(barrel, 190, 100), 0xFF000000u);
    EXPECT_EQ(At(barrel, 100, 100), 0xFFFFFFFFu);
}

TEST_F(FrameDumpTest, VrColorCorrectionDrawsRedLargerThanBlue) {
    auto texture = test::CreateTexture(warp.device.Get(), 8, 8, DXGI_FORMAT_B8G8R8A8_UNORM,
                                       std::vector<uint32_t>(64, 0xFFFFFFFF));
    VrLayout vr;
    vr.k1 = vr.k2 = 0;
    vr.imageScale = 1;
    vr.chromaCorrection = 0.05f;
    // Near the eye's edge red still reaches, blue doesn't.
    uint32_t edge = At(ReadVr(warp.device.Get(), texture.Get(), vr), 197, 100);
    EXPECT_EQ(edge >> 16 & 0xFF, 0xFFu);
    EXPECT_EQ(edge & 0xFF, 0u);
}

// 3D from depth. The picture is 200x20 with each column's x in blue, so a pixel of a view
// tells which column it came from. The depth has a subject in the middle (columns 80..119),
// something nearer at the left (20..39), and a far background. Values are 1/distance-like,
// as reversed depth stores them: background 0.001, subject 0.5, the near thing 1.
class StereoTest : public FrameDumpTest {
protected:
    static constexpr UINT kWidth = 200;
    static constexpr UINT kHeight = 20;

    void SetUp() override {
        FrameDumpTest::SetUp();
        std::vector<uint32_t> columns(kWidth * kHeight);
        for (UINT i = 0; i < kWidth * kHeight; ++i) columns[i] = 0xFF00FF00 | (i % kWidth);
        color = test::CreateTexture(warp.device.Get(), kWidth, kHeight,
                                    DXGI_FORMAT_B8G8R8A8_UNORM, columns);
        vr.enabled = true;
        vr.stereo = true;
        vr.stereoDepth = 0.1f;  // 10 source columns each way far away
        vr.stereoPopOut = 0.5f;
    }

    // 1/distance-like nearness of each column.
    float Nearness(UINT x) const {
        return x >= 80 && x < 120 ? subject : x >= 20 && x < 40 ? 1.0f : 0.001f;
    }

    // Depth as the hook sends it: a 32-bit word a texel of an RGBA8 texture.
    DepthFrame Depth(std::vector<uint32_t> words, DepthEncoding encoding) {
        depthTexture = test::CreateTexture(warp.device.Get(), kWidth, kHeight,
                                           DXGI_FORMAT_R8G8B8A8_UNORM, words);
        DepthFrame depth;
        depth.texture = depthTexture.Get();
        depth.width = kWidth;
        depth.height = kHeight;
        depth.encoding = encoding;
        return depth;
    }

    // As floats, reversed (1 near, 0 far) or the other way around.
    DepthFrame FloatDepth(bool reversed) {
        std::vector<uint32_t> words(kWidth * kHeight);
        for (UINT i = 0; i < kWidth * kHeight; ++i) {
            float d = reversed ? Nearness(i % kWidth) : 1 - Nearness(i % kWidth);
            memcpy(&words[i], &d, 4);
        }
        return Depth(words, kDepthFloat);
    }

    std::vector<uint32_t> Read(const DepthFrame* depth, bool& stereo, UINT maxWidth = 1920,
                               UINT maxHeight = 1080) {
        reader.SetMaxSize(maxWidth, maxHeight);
        reader.SetVrLayout(vr);
        std::vector<uint8_t> bgra;
        std::wstring error;
        EXPECT_TRUE(reader.Read(warp.device.Get(), color.Get(), width, height, bgra, error,
                                depth))
            << error;
        stereo = reader.Stereo();
        std::vector<uint32_t> pixels(bgra.size() / 4);
        memcpy(pixels.data(), bgra.data(), bgra.size());
        return pixels;
    }

    // The source column a view's pixel shows: eye 0 is the left half, each 100 wide, so
    // source column x is at about x / 2.
    int SourceColumn(const std::vector<uint32_t>& frame, int eye, int x) {
        return int(frame[10 * width + eye * (width / 2) + x] & 0xFF);
    }

    // The scene as a pair of eyes converged on the subject would see it.
    void ExpectConvergedOnTheSubject(const std::vector<uint32_t>& frame) {
        // The subject stays put.
        EXPECT_NEAR(SourceColumn(frame, 0, 50) - SourceColumn(frame, 1, 50), 0, 3);
        // Far away goes behind the screen: the left eye sees it further left in its view than
        // the right eye does, so at the same place in the view, a column further right.
        int left = SourceColumn(frame, 0, 85), right = SourceColumn(frame, 1, 85);
        EXPECT_GT(left - right, 14) << left << " " << right;
        // Nearer than the subject comes out, the other way around, but only half as far.
        left = SourceColumn(frame, 0, 15);
        right = SourceColumn(frame, 1, 15);
        EXPECT_LT(left - right, -6) << left << " " << right;
        EXPECT_GT(left - right, -14) << left << " " << right;
    }

    VrLayout vr;
    TextureReader reader;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> color;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;
    UINT width = 0, height = 0;
    float subject = 0.5f;
};

TEST_F(StereoTest, PairOfViewsSideBySideInTheFramesShape) {
    DepthFrame depth = FloatDepth(true);
    bool stereo = false;
    std::vector<uint32_t> frame = Read(&depth, stereo);
    EXPECT_TRUE(stereo);
    EXPECT_EQ(width, kWidth);
    EXPECT_EQ(height, kHeight);
    // Each view squeezes the whole picture into its half.
    EXPECT_NEAR(SourceColumn(frame, 0, 50), 100, 3);
    EXPECT_NEAR(SourceColumn(frame, 1, 50), 100, 3);
}

TEST_F(StereoTest, ConvergesOnTheSubjectInTheMiddle) {
    DepthFrame depth = FloatDepth(true);  // reversed, as most games now have it
    bool stereo = false;
    std::vector<uint32_t> frame = Read(&depth, stereo);
    ASSERT_TRUE(stereo);
    ExpectConvergedOnTheSubject(frame);
}

TEST_F(StereoTest, OlderDepthOrderWorksTheSame) {
    DepthFrame depth = FloatDepth(false);  // 0 near, 1 far: told apart by the average
    bool stereo = false;
    std::vector<uint32_t> frame = Read(&depth, stereo);
    ASSERT_TRUE(stereo);
    ExpectConvergedOnTheSubject(frame);
}

TEST_F(StereoTest, Unorm24DepthIsReadFromTheLow24Bits) {
    std::vector<uint32_t> words(kWidth * kHeight);
    for (UINT i = 0; i < kWidth * kHeight; ++i) {
        // The stencil in the top byte mustn't matter.
        words[i] = 0xAB000000 | uint32_t(Nearness(i % kWidth) * 0xFFFFFF);
    }
    DepthFrame depth = Depth(words, kDepthUnorm24);
    bool stereo = false;
    std::vector<uint32_t> frame = Read(&depth, stereo);
    ASSERT_TRUE(stereo);
    ExpectConvergedOnTheSubject(frame);
}

TEST_F(StereoTest, WhatOneEyeSeesBehindTheSubjectIsBackground) {
    // Either depth order, from the second frame on, once the reader knows which it is.
    for (bool reversed : {true, false}) {
        reader = TextureReader();
        DepthFrame depth = FloatDepth(reversed);
        bool stereo = false;
        Read(&depth, stereo);
        std::vector<uint32_t> frame = Read(&depth, stereo);
        ASSERT_TRUE(stereo);
        // The background moves out from behind the subject: right of it in the right eye's
        // view, left of it in the left eye's, there's a strip the picture doesn't have. It's
        // filled with background (columns outside 80..119), not the subject's edge pulled out.
        for (int x = 60; x <= 64; ++x) {
            EXPECT_GE(SourceColumn(frame, 1, x), 120) << "right view at " << x << ", " << reversed;
        }
        for (int x = 36; x <= 39; ++x) {
            EXPECT_LE(SourceColumn(frame, 0, x), 79) << "left view at " << x << ", " << reversed;
        }
    }
}

// How far apart the views show the background: what moves when the convergence does.
TEST_F(StereoTest, TheScreenHoldsStillWhileTheSubjectMerelyMoves) {
    auto background = [&](const std::vector<uint32_t>& frame) {
        return SourceColumn(frame, 0, 85) - SourceColumn(frame, 1, 85);
    };
    bool stereo = false;
    DepthFrame depth = FloatDepth(true);
    int steady = background(Read(&depth, stereo));
    for (int frame = 0; frame < 5; ++frame) Read(&depth, stereo);
    EXPECT_EQ(background(Read(&depth, stereo)), steady);

    // A character breathing, turning: a tenth nearer. Nothing moves.
    subject = 0.55f;
    depth = FloatDepth(true);
    for (int frame = 0; frame < 30; ++frame) {
        EXPECT_EQ(background(Read(&depth, stereo)), steady) << frame;
    }

    // Something else in the middle now, twice as near: the 3D follows, but slowly.
    subject = 1.0f;
    depth = FloatDepth(true);
    int before = steady;
    for (int frame = 0; frame < 10; ++frame) {
        int now = background(Read(&depth, stereo));
        EXPECT_LE(std::abs(now - before), 1) << frame;
        before = now;
    }
    for (int frame = 0; frame < 120; ++frame) Read(&depth, stereo);
    EXPECT_LT(background(Read(&depth, stereo)), steady - 2) << "followed in the end";
}

TEST_F(StereoTest, NoDepthOrNo3DMeansOnePicture) {
    bool stereo = true;
    std::vector<uint32_t> frame = Read(nullptr, stereo);
    EXPECT_FALSE(stereo);
    EXPECT_EQ(SourceColumn(frame, 0, 150), 150) << "flat, as it is";

    vr.stereo = false;
    DepthFrame depth = FloatDepth(true);
    frame = Read(&depth, stereo);
    EXPECT_FALSE(stereo);
}

TEST_F(StereoTest, ThePcWarpsAPairIntoEachEye) {
    vr.lensOnPhone = false;
    vr.screenAspect = 2.0f;
    vr.k1 = vr.k2 = 0;
    vr.imageScale = 0.1f;  // the 10:1 picture, 10 px tall: as wide as each 100 px eye
    DepthFrame depth = FloatDepth(true);
    bool stereo = false;
    std::vector<uint32_t> frame = Read(&depth, stereo, 200, 100);
    ASSERT_TRUE(stereo);
    ASSERT_EQ(width, 200u);
    ASSERT_EQ(height, 100u);
    // Each eye has its own view: the background (around column 60 here) further right in
    // the left eye's, as it's behind the subject the views converge on.
    int left = int(frame[50 * width + 30] & 0xFF), right = int(frame[50 * width + 130] & 0xFF);
    EXPECT_EQ(frame[50 * width + 30] & 0xFF00, 0xFF00u) << "the picture, not the black around";
    EXPECT_GT(left - right, 6) << left << " " << right;
}

}  // namespace
}  // namespace gazeora
