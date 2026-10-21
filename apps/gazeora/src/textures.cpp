#include "textures.h"

#include <shellapi.h>
#include <shlobj.h>
#include <wincodec.h>

#include <algorithm>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace gazeora::app {

TextureCache::TextureCache(ID3D11Device* device) : m_device(device) {
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wic));
}

const Texture& TextureCache::Icon(const std::wstring& exePath) {
    auto it = m_icons.find(exePath);
    if (it != m_icons.end()) {
        return it->second;
    }
    HICON icon = nullptr;
    // Asks for the 256 px icon. Exes with only small icons come back padded to that size,
    // which FromWicSource crops off.
    if (FAILED(SHDefExtractIconW(exePath.c_str(), 0, 0, &icon, nullptr, 256)) || !icon) {
        SHFILEINFOW info = {};
        SHGetFileInfoW(exePath.c_str(), 0, &info, sizeof(info), SHGFI_ICON | SHGFI_LARGEICON);
        icon = info.hIcon;
    }
    Texture texture;
    if (icon) {
        texture = FromHIcon(icon);
        DestroyIcon(icon);
    }
    return m_icons[exePath] = texture;
}

const Texture& TextureCache::Image(const std::wstring& path) {
    auto it = m_images.find(path);
    if (it != m_images.end()) {
        return it->second;
    }
    Texture texture;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (m_wic &&
        SUCCEEDED(m_wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                   WICDecodeMetadataCacheOnDemand, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame))) {
        texture = FromWicSource(frame.Get(), false);
    }
    return m_images[path] = texture;
}

const Texture& TextureCache::Gradient(bool vertical) {
    Texture& texture = m_gradients[vertical ? 1 : 0];
    if (!texture) {
        constexpr int kSteps = 256;
        uint32_t pixels[kSteps];
        for (int i = 0; i < kSteps; ++i) {
            // Smoothstep falloff looks less banded than a linear ramp.
            float t = float(i) / float(kSteps - 1);
            float alpha = 1.0f - t * t * (3.0f - 2.0f * t);
            pixels[i] = (uint32_t(alpha * 255.0f + 0.5f) << 24) | 0x00FFFFFF;
        }
        texture = vertical ? Create(1, kSteps, pixels) : Create(kSteps, 1, pixels);
    }
    return texture;
}

void TextureCache::Forget(const std::wstring& path) {
    m_icons.erase(path);
    m_images.erase(path);
}

Texture TextureCache::Create(int width, int height, const uint32_t* rgba) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = UINT(width);
    desc.Height = UINT(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data = {rgba, UINT(width) * 4, 0};

    Texture texture;
    ComPtr<ID3D11Texture2D> resource;
    if (SUCCEEDED(m_device->CreateTexture2D(&desc, &data, &resource)) &&
        SUCCEEDED(m_device->CreateShaderResourceView(resource.Get(), nullptr, &texture.view))) {
        texture.width = width;
        texture.height = height;
    }
    return texture;
}

Texture TextureCache::FromHIcon(HICON icon) {
    ComPtr<IWICBitmap> bitmap;
    if (!m_wic || FAILED(m_wic->CreateBitmapFromHICON(icon, &bitmap))) {
        return {};
    }
    return FromWicSource(bitmap.Get(), true);
}

Texture TextureCache::FromWicSource(IWICBitmapSource* source, bool cropToAlpha) {
    ComPtr<IWICFormatConverter> converter;
    UINT width = 0, height = 0;
    if (FAILED(m_wic->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(source, GUID_WICPixelFormat32bppRGBA,
                                     WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom)) ||
        FAILED(converter->GetSize(&width, &height)) || width == 0 || height == 0) {
        return {};
    }
    std::vector<uint32_t> pixels(size_t(width) * height);
    if (FAILED(converter->CopyPixels(nullptr, width * 4, UINT(pixels.size() * 4),
                                     reinterpret_cast<BYTE*>(pixels.data())))) {
        return {};
    }

    // Small icons come back in the corner of a 256 px canvas; keep just the visible part.
    int left = 0, top = 0, right = int(width), bottom = int(height);
    if (cropToAlpha) {
        left = int(width), top = int(height), right = 0, bottom = 0;
        for (int y = 0; y < int(height); ++y) {
            for (int x = 0; x < int(width); ++x) {
                if ((pixels[size_t(y) * width + x] >> 24) > 8) {
                    left = std::min(left, x);
                    right = std::max(right, x + 1);
                    top = std::min(top, y);
                    bottom = std::max(bottom, y + 1);
                }
            }
        }
        if (right <= left || bottom <= top) {
            return {};
        }
        // Keep it square so the icon isn't stretched.
        int side = std::max(right - left, bottom - top);
        right = std::min(int(width), left + side);
        bottom = std::min(int(height), top + side);
    }

    int outWidth = right - left, outHeight = bottom - top;
    std::vector<uint32_t> cropped(size_t(outWidth) * outHeight);
    double sum[3] = {}, weight = 0.0;
    for (int y = 0; y < outHeight; ++y) {
        for (int x = 0; x < outWidth; ++x) {
            uint32_t p = pixels[size_t(y + top) * width + (x + left)];
            cropped[size_t(y) * outWidth + x] = p;
            double a = double(p >> 24) / 255.0;
            sum[0] += a * double(p & 0xFF);
            sum[1] += a * double((p >> 8) & 0xFF);
            sum[2] += a * double((p >> 16) & 0xFF);
            weight += a;
        }
    }

    Texture texture = Create(outWidth, outHeight, cropped.data());
    if (weight > 0.0) {
        for (int c = 0; c < 3; ++c) {
            texture.averageColor[c] = float(sum[c] / weight / 255.0);
        }
    }
    return texture;
}

}  // namespace gazeora::app
