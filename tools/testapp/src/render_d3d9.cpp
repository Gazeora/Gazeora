#include <d3d9.h>
#include <wrl/client.h>

#include <cstdio>

#include "testapp/app.h"

using Microsoft::WRL::ComPtr;

namespace testapp {

namespace {

D3DCOLOR ToD3DColor(const float c[4]) { return D3DCOLOR_COLORVALUE(c[0], c[1], c[2], c[3]); }

}  // namespace

int RunD3D9(HWND hwnd) {
    // A plain (non-Ex) device: what most D3D9 games use.
    ComPtr<IDirect3D9> d3d;
    d3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
    if (!d3d) {
        fprintf(stderr, "Direct3DCreate9 failed\n");
        return 1;
    }
    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth = kWidth;
    pp.BackBufferHeight = kHeight;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = hwnd;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    ComPtr<IDirect3DDevice9> device;
    if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                 D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device))) {
        fprintf(stderr, "IDirect3D9::CreateDevice failed\n");
        return 1;
    }

    Clock clock;
    while (PumpMessages()) {
        Scene scene = SceneAt(clock.Seconds());
        D3DRECT square = {scene.square.left, scene.square.top, scene.square.right,
                          scene.square.bottom};
        device->Clear(0, nullptr, D3DCLEAR_TARGET, ToD3DColor(scene.background), 1.0f, 0);
        device->Clear(1, &square, D3DCLEAR_TARGET, ToD3DColor(kWhite), 1.0f, 0);
        device->Present(nullptr, nullptr, nullptr, nullptr);
    }
    return 0;
}

}  // namespace testapp
