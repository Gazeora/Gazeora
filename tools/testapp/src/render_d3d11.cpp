// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstring>

#include "testapp/app.h"
#include "testapp/mouse_look.h"

using Microsoft::WRL::ComPtr;

namespace testapp {

int RunD3D11(HWND hwnd) {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                 D3D11_SDK_VERSION, &device, nullptr, &context))) {
        fprintf(stderr, "D3D11CreateDevice failed\n");
        return 1;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    device.As(&dxgiDevice);
    dxgiDevice->GetAdapter(&adapter);
    adapter->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = kWidth;
    sd.Height = kHeight;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> swap;
    if (FAILED(factory->CreateSwapChainForHwnd(device.Get(), hwnd, &sd, nullptr, nullptr, &swap))) {
        fprintf(stderr, "CreateSwapChainForHwnd failed\n");
        return 1;
    }

    ComPtr<ID3D11Texture2D> backBuffer;
    ComPtr<ID3D11RenderTargetView> rtv;
    swap->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    device->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);
    ComPtr<ID3D11DeviceContext1> context1;
    context.As(&context1);

    // The 3D scene, with a depth buffer the way older games have it: 24-bit, 0 near and 1 far,
    // and not readable by shaders.
    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width = kWidth;
    dd.Height = kHeight;
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3DBlob> vsCode, psCode;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    D3D11_DEPTH_STENCIL_DESC ds = {};
    ds.DepthEnable = TRUE;
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D11_COMPARISON_LESS;
    ComPtr<ID3D11DepthStencilState> depthState;
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> rasterizer;
    D3D11_BUFFER_DESC cd = {};
    cd.ByteWidth = sizeof(SceneConstants);
    cd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> constants;
    if (FAILED(device->CreateTexture2D(&dd, nullptr, &depth)) ||
        FAILED(device->CreateDepthStencilView(depth.Get(), nullptr, &dsv)) ||
        FAILED(D3DCompile(kSceneShader, strlen(kSceneShader), nullptr, nullptr, nullptr, "vs",
                          "vs_4_0", 0, 0, &vsCode, nullptr)) ||
        FAILED(D3DCompile(kSceneShader, strlen(kSceneShader), nullptr, nullptr, nullptr, "ps",
                          "ps_4_0", 0, 0, &psCode, nullptr)) ||
        FAILED(device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
                                          nullptr, &vs)) ||
        FAILED(device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(),
                                         nullptr, &ps)) ||
        FAILED(device->CreateDepthStencilState(&ds, &depthState)) ||
        FAILED(device->CreateRasterizerState(&rd, &rasterizer)) ||
        FAILED(device->CreateBuffer(&cd, nullptr, &constants))) {
        fprintf(stderr, "creating the 3D scene failed\n");
        return 1;
    }
    D3D11_VIEWPORT viewport = {0, 0, float(kWidth), float(kHeight), 0, 1};

    MouseLook* look = MouseLook::Of(hwnd);
    Clock clock;
    while (PumpMessages()) {
        float t = clock.Seconds();
        Scene scene = SceneAt(t);
        context->ClearRenderTargetView(rtv.Get(), scene.background);
        context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        SceneConstants values = {t, 0.0f, float(kWidth) / kHeight};
        if (look) {
            values.yaw = look->YawRadians();
            values.pitch = look->PitchRadians();
            look->ShowInTitle(hwnd);
        }
        context->UpdateSubresource(constants.Get(), 0, nullptr, &values, 0, 0);
        context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
        context->OMSetDepthStencilState(depthState.Get(), 0);
        context->RSSetState(rasterizer.Get());
        context->RSSetViewports(1, &viewport);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context->VSSetShader(vs.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, constants.GetAddressOf());
        context->PSSetShader(ps.Get(), nullptr, 0);
        context->DrawInstanced(4, kSceneInstances, 0, 0);
        // A HUD-like square on top, drawn without depth.
        if (context1) {
            context1->ClearView(rtv.Get(), kWhite, &scene.square, 1);
        }
        swap->Present(1, 0);
    }
    return 0;
}

}  // namespace testapp
