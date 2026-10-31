// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/dxgi/dxgi_hooks.h"

// d3d10_1.h, which includes d3d10.h: the Windows SDK refuses d3d10_1.h after d3d10.h, and
// another of its headers here brings it in.
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "gazeora/hook/core/capture_target.h"
#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/core/log.h"
#include "gazeora/hook/d3d10/d3d10_capture.h"
#include "gazeora/hook/d3d11/d3d11_capture.h"
#include "gazeora/hook/d3d11/d3d11_depth.h"
#include "gazeora/hook/d3d12/d3d12_capture.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook::dxgi {

namespace {

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT,
                                               const DXGI_PRESENT_PARAMETERS*);

// vtable slots: IUnknown 0-2, IDXGIObject 3-6, IDXGIDeviceSubObject 7,
// IDXGISwapChain 8-17, IDXGISwapChain1 18-28.
constexpr size_t kPresentSlot = 8;
constexpr size_t kPresent1Slot = 22;

PresentFn g_origPresent = nullptr;
Present1Fn g_origPresent1 = nullptr;

// Present1 may call Present internally; don't capture the same frame twice.
thread_local bool t_inPresent = false;

// Only touched under a CaptureScope.
D3D10Capture g_d3d10;
D3D11Capture g_d3d11;
D3D12Capture g_d3d12;
IDXGISwapChain* g_lastSwap = nullptr;  // identity only, not AddRef'd
bool g_reportedUnsupported = false;

void CaptureFrame(IDXGISwapChain* swap) {
    CaptureScope scope;
    if (!scope.Active()) {
        return;
    }

    // A D3D10 device may also answer to ID3D11Device, at a feature level below 11_0,
    // so check for D3D10 before accepting those.
    ComPtr<ID3D11Device> d3d11;
    ComPtr<ID3D10Device> d3d10;
    ComPtr<ID3D12Device> d3d12;
    if (SUCCEEDED(swap->GetDevice(IID_PPV_ARGS(&d3d11))) &&
        d3d11->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0 &&
        SUCCEEDED(swap->GetDevice(IID_PPV_ARGS(&d3d10)))) {
        d3d11.Reset();
    }
    if (!d3d11 && !d3d10 && FAILED(swap->GetDevice(IID_PPV_ARGS(&d3d10))) &&
        FAILED(swap->GetDevice(IID_PPV_ARGS(&d3d12)))) {
        if (!g_reportedUnsupported) {
            g_reportedUnsupported = true;
            SetStatus("ignoring a swapchain that isn't D3D10, D3D11 or D3D12");
        }
        return;
    }

    if (!scope.SelectTarget(swap)) {
        return;
    }
    if (g_lastSwap != swap) {
        g_lastSwap = swap;
        g_d3d10.Reset();
        g_d3d11.Reset();
        g_d3d12.Reset();
    }
    if (d3d11) {
        g_d3d11.Capture(swap, d3d11.Get(), scope.PresentQpc());
    } else if (d3d10) {
        g_d3d10.Capture(swap, d3d10.Get(), scope.PresentQpc());
    } else {
        g_d3d12.Capture(swap, d3d12.Get(), scope.PresentQpc());
    }
}

void OnPresent(IDXGISwapChain* swap, UINT flags) {
    if (t_inPresent || (flags & DXGI_PRESENT_TEST)) {
        return;
    }
    CaptureFrame(swap);
}

HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* swap, UINT syncInterval, UINT flags) {
    OnPresent(swap, flags);
    bool outer = !t_inPresent;
    t_inPresent = true;
    HRESULT hr = g_origPresent(swap, syncInterval, flags);
    if (outer) {
        t_inPresent = false;
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE HookPresent1(IDXGISwapChain1* swap, UINT syncInterval, UINT flags,
                                       const DXGI_PRESENT_PARAMETERS* params) {
    OnPresent(swap, flags);
    bool outer = !t_inPresent;
    t_inPresent = true;
    HRESULT hr = g_origPresent1(swap, syncInterval, flags, params);
    if (outer) {
        t_inPresent = false;
    }
    return hr;
}

}  // namespace

// Reads the swapchain vtable from a throwaway D3D11 device and swapchain. Every
// swapchain dxgi.dll creates shares that vtable, so hooking it catches the game's.
bool Install() {
    DummyWindow window;
    if (!window.hwnd()) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 16;
    sd.BufferDesc.Height = 16;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = window.hwnd();
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11Device> device;
    HRESULT hr = E_FAIL;
    for (D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, type, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                           &sd, &swap, &device, nullptr, nullptr);
        if (SUCCEEDED(hr)) {
            break;
        }
    }
    if (FAILED(hr)) {
        SetStatus("dummy D3D11CreateDeviceAndSwapChain failed: 0x%08lX", hr);
        return false;
    }

    if (!CreateHook("IDXGISwapChain::Present", VTableEntry(swap.Get(), kPresentSlot), &HookPresent,
                    &g_origPresent)) {
        return false;
    }
    ComPtr<IDXGISwapChain1> swap1;
    if (SUCCEEDED(swap.As(&swap1)) &&
        !CreateHook("IDXGISwapChain1::Present1", VTableEntry(swap1.Get(), kPresent1Slot),
                    &HookPresent1, &g_origPresent1)) {
        return false;
    }
    // Without them, D3D11 games are captured in 2D only.
    d3d11::InstallDepthHooks(device.Get());
    return true;
}

}  // namespace gazeora::hook::dxgi
