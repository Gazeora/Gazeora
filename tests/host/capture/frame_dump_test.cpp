#include "gazeora/host/capture/frame_dump.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <cstdio>
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

}  // namespace
}  // namespace gazeora
