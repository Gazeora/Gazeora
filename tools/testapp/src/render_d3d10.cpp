// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <d3d10.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstdio>
#include <vector>

#include "testapp/app.h"

using Microsoft::WRL::ComPtr;

namespace testapp {

namespace {

constexpr UINT kSquareSize = 200;

}  // namespace

int RunD3D10(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = kWidth;
    sd.BufferDesc.Height = kHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D10Device> device;
    if (FAILED(D3D10CreateDeviceAndSwapChain(nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                             D3D10_SDK_VERSION, &sd, &swap, &device))) {
        fprintf(stderr, "D3D10CreateDeviceAndSwapChain failed\n");
        return 1;
    }

    ComPtr<ID3D10Texture2D> backBuffer;
    ComPtr<ID3D10RenderTargetView> rtv;
    swap->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    device->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);

    // D3D10 can't clear a rectangle, so the square is copied in from a white texture.
    std::vector<uint32_t> white(kSquareSize * kSquareSize, 0xFFFFFFFF);
    D3D10_TEXTURE2D_DESC td = {};
    td.Width = td.Height = kSquareSize;
    td.MipLevels = td.ArraySize = 1;
    td.Format = sd.BufferDesc.Format;
    td.SampleDesc.Count = 1;
    td.Usage = D3D10_USAGE_IMMUTABLE;
    td.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    D3D10_SUBRESOURCE_DATA init = {white.data(), kSquareSize * 4, 0};
    ComPtr<ID3D10Texture2D> square;
    if (FAILED(device->CreateTexture2D(&td, &init, &square))) {
        fprintf(stderr, "CreateTexture2D failed\n");
        return 1;
    }

    Clock clock;
    while (PumpMessages()) {
        Scene scene = SceneAt(clock.Seconds());
        device->ClearRenderTargetView(rtv.Get(), scene.background);
        device->CopySubresourceRegion(backBuffer.Get(), 0, UINT(scene.square.left),
                                      UINT(scene.square.top), 0, square.Get(), 0, nullptr);
        swap->Present(1, 0);
    }
    return 0;
}

}  // namespace testapp
