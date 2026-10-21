#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstdio>

#include "testapp/app.h"

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

    Clock clock;
    while (PumpMessages()) {
        Scene scene = SceneAt(clock.Seconds());
        context->ClearRenderTargetView(rtv.Get(), scene.background);
        if (context1) {
            context1->ClearView(rtv.Get(), kWhite, &scene.square, 1);
        }
        swap->Present(1, 0);
    }
    return 0;
}

}  // namespace testapp
