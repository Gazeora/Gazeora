#include "gazeora/host/encode/gpu_nv12_converter.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "gazeora/host/capture/frame_dump.h"
#include "gazeora/host/encode/color_convert.h"
#include "gazeora/host/encode/nv12_readback.h"
#include "support/d3d_test_util.h"

using Microsoft::WRL::ComPtr;

namespace gazeora {
namespace {

class GpuNv12ConverterTest : public ::testing::Test {
protected:
    void SetUp() override {
        warp = test::CreateWarpDevice();
        ASSERT_TRUE(warp.device) << "WARP D3D11.4 device unavailable";
    }

    // A width x height texture of `format` filled with noise.
    ComPtr<ID3D11Texture2D> Noise(UINT width, UINT height, DXGI_FORMAT format) {
        std::vector<uint32_t> pixels(size_t(width) * height);
        uint32_t seed = 7;
        for (uint32_t& p : pixels) p = seed = seed * 1664525u + 1013904223u;
        return test::CreateTexture(warp.device.Get(), width, height, format, pixels);
    }

    // What the CPU path streams for `texture`: read back as BGRA8, then BgraToNv12.
    std::vector<uint8_t> CpuNv12(ID3D11Texture2D* texture) {
        TextureReader reader;
        UINT width = 0, height = 0;
        std::vector<uint8_t> bgra, nv12;
        std::wstring error;
        EXPECT_TRUE(reader.Read(warp.device.Get(), texture, width, height, bgra, error));
        BgraToNv12(bgra.data(), width, width & ~1u, height & ~1u, nv12);
        return nv12;
    }

    // The same through GpuNv12Converter, read back from the GPU.
    std::vector<uint8_t> GpuNv12(GpuNv12Converter& converter, ID3D11Texture2D* texture) {
        UINT width = 0, height = 0;
        std::wstring error;
        ComPtr<ID3D11Texture2D> nv12 =
            converter.Convert(warp.device.Get(), texture, width, height, error);
        EXPECT_TRUE(nv12) << std::string(error.begin(), error.end());
        std::vector<uint8_t> bytes;
        if (nv12) {
            Nv12Readback readback;
            EXPECT_TRUE(readback.Read(nv12.Get(), width, height, bytes, error));
        }
        return bytes;
    }

    int MaxDifference(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
        EXPECT_EQ(a.size(), b.size());
        int worst = 0;
        for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
            worst = std::max(worst, std::abs(int(a[i]) - int(b[i])));
        }
        return worst;
    }

    test::WarpDevice warp;
};

TEST_F(GpuNv12ConverterTest, GivesTheCpuPathsBytesForEachFormat) {
    GpuNv12Converter converter;
    for (DXGI_FORMAT format : {DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
                               DXGI_FORMAT_R10G10B10A2_UNORM}) {
        ComPtr<ID3D11Texture2D> texture = Noise(66, 34, format);
        EXPECT_EQ(MaxDifference(GpuNv12(converter, texture.Get()), CpuNv12(texture.Get())), 0)
            << "DXGI format " << format;
    }
}

TEST_F(GpuNv12ConverterTest, KeepsSrgbFramesGammaEncoded) {
    // The CPU path copies the bytes as they are; the GPU reads linear light and encodes it
    // back, which may round a byte the other way.
    GpuNv12Converter converter;
    ComPtr<ID3D11Texture2D> texture = Noise(64, 32, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
    EXPECT_LE(MaxDifference(GpuNv12(converter, texture.Get()), CpuNv12(texture.Get())), 1);
}

TEST_F(GpuNv12ConverterTest, DropsTheOddLastRowAndColumn) {
    GpuNv12Converter converter;
    ComPtr<ID3D11Texture2D> texture = Noise(67, 35, DXGI_FORMAT_B8G8R8A8_UNORM);
    UINT width = 0, height = 0;
    std::wstring error;
    ASSERT_TRUE(converter.Convert(warp.device.Get(), texture.Get(), width, height, error));
    EXPECT_EQ(width, 66u);
    EXPECT_EQ(height, 34u);
    EXPECT_EQ(MaxDifference(GpuNv12(converter, texture.Get()), CpuNv12(texture.Get())), 0);
}

TEST_F(GpuNv12ConverterTest, TakesTexturesFromThePoolInTurn) {
    GpuNv12Converter converter;
    ComPtr<ID3D11Texture2D> texture = Noise(16, 16, DXGI_FORMAT_B8G8R8A8_UNORM);
    std::vector<ID3D11Texture2D*> used;
    for (size_t i = 0; i < Nv12TexturePool::kSize * 2; ++i) {
        UINT width = 0, height = 0;
        std::wstring error;
        used.push_back(converter.Convert(warp.device.Get(), texture.Get(), width, height, error).Get());
    }
    // A texture comes round again only after the whole pool.
    std::set<ID3D11Texture2D*> distinct(used.begin(), used.begin() + Nv12TexturePool::kSize);
    EXPECT_EQ(distinct.size(), Nv12TexturePool::kSize);
    for (size_t i = 0; i < Nv12TexturePool::kSize; ++i) {
        EXPECT_EQ(used[i], used[i + Nv12TexturePool::kSize]);
    }
}

TEST_F(GpuNv12ConverterTest, RefusesFormatsItCantRead) {
    GpuNv12Converter converter;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = desc.Height = 4;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> texture;
    ASSERT_TRUE(SUCCEEDED(warp.device->CreateTexture2D(&desc, nullptr, &texture)));
    UINT width = 0, height = 0;
    std::wstring error;
    EXPECT_FALSE(converter.Convert(warp.device.Get(), texture.Get(), width, height, error));
    EXPECT_FALSE(error.empty());
}

}  // namespace
}  // namespace gazeora
