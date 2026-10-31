// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdint>
#include <map>
#include <string>

namespace gazeora::app {

struct Texture {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    int width = 0;
    int height = 0;
    float averageColor[3] = {0.2f, 0.2f, 0.2f};  // alpha-weighted average, 0..1

    explicit operator bool() const { return view != nullptr; }
    uint64_t Id() const { return reinterpret_cast<uintptr_t>(view.Get()); }
    float Aspect() const { return height > 0 ? float(width) / float(height) : 1.0f; }
};

// Loads game icons and cover images into textures, once per path.
class TextureCache {
public:
    explicit TextureCache(ID3D11Device* device);

    // The exe's largest icon, cropped to its visible pixels. Empty texture on failure.
    const Texture& Icon(const std::wstring& exePath);
    // Any image WIC can decode (PNG, JPEG, BMP, ...). Empty texture on failure.
    const Texture& Image(const std::wstring& path);
    // White with alpha falling 1 -> 0 along the texture: vertical (top to bottom) or
    // horizontal (left to right). Tinted when drawn, for rounded gradients.
    const Texture& Gradient(bool vertical);

    // Drop a path, e.g. after the user picked a new cover.
    void Forget(const std::wstring& path);

private:
    Texture Create(int width, int height, const uint32_t* rgba);
    Texture FromHIcon(HICON icon);
    Texture FromWicSource(IWICBitmapSource* source, bool cropToAlpha);

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<IWICImagingFactory> m_wic;
    std::map<std::wstring, Texture> m_icons;
    std::map<std::wstring, Texture> m_images;
    Texture m_gradients[2];
};

}  // namespace gazeora::app
