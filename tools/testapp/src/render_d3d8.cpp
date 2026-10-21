// Only built when the toolchain ships d3d8.h (MinGW does; the Windows SDK doesn't).
#include <d3d8.h>
#include <wrl/client.h>

#include <cstdio>

#include "testapp/app.h"

using Microsoft::WRL::ComPtr;

namespace testapp {

namespace {

D3DCOLOR ToD3DColor(const float c[4]) {
    return D3DCOLOR_COLORVALUE(c[0], c[1], c[2], c[3]);
}

}  // namespace

int RunD3D8(HWND hwnd) {
    // No import library ships for d3d8.dll, so load it like old games' wrappers do.
    using Direct3DCreate8Fn = IDirect3D8*(WINAPI*)(UINT);
    HMODULE module = LoadLibraryW(L"d3d8.dll");
    auto create = module ? reinterpret_cast<Direct3DCreate8Fn>(
                               reinterpret_cast<void*>(GetProcAddress(module, "Direct3DCreate8")))
                         : nullptr;
    ComPtr<IDirect3D8> d3d;
    if (create) {
        d3d.Attach(create(D3D_SDK_VERSION));
    }
    if (!d3d) {
        fprintf(stderr, "Direct3DCreate8 failed\n");
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
    ComPtr<IDirect3DDevice8> device;
    if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                 D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device))) {
        fprintf(stderr, "IDirect3D8::CreateDevice failed\n");
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
        Sleep(1);  // windowed D3D8 has no vsync; don't spin a core
    }
    return 0;
}

}  // namespace testapp
