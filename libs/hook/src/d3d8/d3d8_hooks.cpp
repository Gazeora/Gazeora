// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/d3d8/d3d8_hooks.h"

#include <wrl/client.h>

#include "gazeora/hook/core/capture_target.h"
#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/core/log.h"
#include "gazeora/hook/d3d8/d3d8_api.h"
#include "gazeora/hook/d3d8/d3d8_capture.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook::d3d8 {

namespace {

PresentFn g_origPresent = nullptr;
ResetFn g_origReset = nullptr;
Direct3DCreate8Fn g_origDirect3DCreate8 = nullptr;
volatile LONG g_vtablesHooked = 0;

// Only touched under a CaptureScope or TeardownLock.
D3D8Capture g_capture;

HRESULT STDMETHODCALLTYPE HookPresent(Device8* device, const RECT* src, const RECT* dst,
                                      HWND window, const RGNDATA* dirty) {
    {
        CaptureScope scope;
        if (scope.Active() && scope.SelectTarget(device)) {
            g_capture.Capture(device, scope.PresentQpc());
        }
    }
    return g_origPresent(device, src, dst, window, dirty);
}

HRESULT STDMETHODCALLTYPE HookReset(Device8* device, PresentParameters* params) {
    {
        TeardownLock lock;
        if (g_capture.Device() == device) {
            g_capture.Reset();
        }
    }
    return g_origReset(device, params);
}

// Creates a throwaway device to read the IDirect3DDevice8 vtable from.
template <typename CreateFn>
bool HookVTable(Direct3DCreate8Fn direct3DCreate8, CreateFn create) {
    DummyWindow window;
    if (!window.hwnd()) {
        return false;
    }
    ComPtr<Direct3D8> d3d;
    d3d.Attach(direct3DCreate8(kSdkVersion));
    if (!d3d) {
        SetStatus("dummy Direct3DCreate8 failed");
        return false;
    }
    // A windowed D3D8 device takes the desktop's format only: unlike D3D9, it refuses
    // D3DFMT_UNKNOWN for it.
    D3DDISPLAYMODE mode = {};
    if (FAILED(GetAdapterDisplayMode(d3d.Get(), kAdapterDefault, &mode))) {
        SetStatus("dummy D3D8 device: no display mode");
        return false;
    }
    // Explicit size: the dummy window's client area may be empty.
    PresentParameters pp = {};
    pp.BackBufferWidth = 16;
    pp.BackBufferHeight = 16;
    pp.BackBufferCount = 1;
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = mode.Format;
    pp.hDeviceWindow = window.hwnd();
    ComPtr<Device8> device;
    HRESULT hr = E_FAIL;
    for (D3DDEVTYPE type : {D3DDEVTYPE_HAL, D3DDEVTYPE_REF}) {
        hr = CreateDevice(d3d.Get(), kAdapterDefault, type, window.hwnd(),
                          kCreateSoftwareVertexProcessing, &pp, &device);
        if (SUCCEEDED(hr)) {
            break;
        }
    }
    if (FAILED(hr)) {
        SetStatus("dummy D3D8 device creation failed: 0x%08lX", hr);
        return false;
    }
    return create("IDirect3DDevice8::Present", VTableEntry(device.Get(), kPresentSlot),
                  &HookPresent, &g_origPresent) &&
           create("IDirect3DDevice8::Reset", VTableEntry(device.Get(), kResetSlot), &HookReset,
                  &g_origReset);
}

// Launched games: hook the vtable when the game first creates a Direct3D 8 object.
Direct3D8* WINAPI HookDirect3DCreate8(UINT sdkVersion) {
    Direct3D8* d3d = g_origDirect3DCreate8(sdkVersion);
    if (d3d && InterlockedCompareExchange(&g_vtablesHooked, 1, 0) == 0 &&
        HookVTable(g_origDirect3DCreate8,
                   [](const char* name, void* target, auto detour, auto* original) {
                       return CreateEnabledHook(name, target, detour, original);
                   })) {
        SetStatus("hooked D3D8 on first use, waiting for frames");
    }
    return d3d;
}

}  // namespace

bool Install() {
    HMODULE module = GetModuleHandleW(L"d3d8.dll");
    if (module) {
        auto create = reinterpret_cast<Direct3DCreate8Fn>(ModuleExport(module, "Direct3DCreate8"));
        return create &&
               HookVTable(create, [](const char* name, void* target, auto detour, auto* original) {
                   return CreateHook(name, target, detour, original);
               });
    }
    module = GraphicsRuntime(L"d3d8.dll");
    void* create = ModuleExport(module, "Direct3DCreate8");
    return create &&
           CreateHook("Direct3DCreate8", create, &HookDirect3DCreate8, &g_origDirect3DCreate8);
}

}  // namespace gazeora::hook::d3d8
