// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/d3d11/d3d11_frame_ring.h"

#include "gazeora/hook/core/log.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook {

void D3D11FrameRing::Release() {
    m_handles.Close();
    for (auto& slot : m_slots) slot.Reset();
    for (auto& slot : m_depthSlots) slot.Reset();
    m_frameFence.Reset();
    m_hostFence.Reset();
    m_width = m_height = 0;
    m_format = DXGI_FORMAT_UNKNOWN;
}

bool D3D11FrameRing::Matches(UINT width, UINT height, DXGI_FORMAT format, bool depth) const {
    return m_frameFence && width == m_width && height == m_height && format == m_format &&
           (m_depthSlots[0] != nullptr) == depth && IsPublisher(this);
}

bool D3D11FrameRing::Create(ID3D11Device5* device, UINT width, UINT height, DXGI_FORMAT format,
                            GraphicsApi api, bool depth) {
    Release();

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

    auto createShared = [&](const D3D11_TEXTURE2D_DESC& desc, ComPtr<ID3D11Texture2D>& slot,
                            HANDLE& handle) {
        ComPtr<IDXGIResource1> resource;
        HRESULT hr = device->CreateTexture2D(&desc, nullptr, &slot);
        if (SUCCEEDED(hr)) hr = slot.As(&resource);
        if (SUCCEEDED(hr)) {
            hr = resource->CreateSharedHandle(
                nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &handle);
        }
        if (FAILED(hr)) {
            SetStatus("D3D11 shared texture %ux%u fmt %d failed: 0x%08lX", desc.Width, desc.Height,
                      desc.Format, hr);
        }
        return SUCCEEDED(hr);
    };
    // Depth values travel as 32-bit words in RGBA8, a format every GPU shares.
    D3D11_TEXTURE2D_DESC depthDesc = desc;
    depthDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    for (UINT i = 0; i < kFrameSlots; ++i) {
        if (!createShared(desc, m_slots[i], m_handles.slots[i]) ||
            (depth && !createShared(depthDesc, m_depthSlots[i], m_handles.depthSlots[i]))) {
            Release();
            return false;
        }
    }

    UINT64 initial = InitialFenceValue();
    HRESULT hr = device->CreateFence(initial, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_frameFence));
    if (SUCCEEDED(hr)) {
        hr = device->CreateFence(initial, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_hostFence));
    }
    if (SUCCEEDED(hr)) {
        hr = m_frameFence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &m_handles.frameFence);
    }
    if (SUCCEEDED(hr)) {
        hr = m_hostFence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &m_handles.hostFence);
    }
    if (FAILED(hr)) {
        SetStatus("D3D11 shared fence creation failed: 0x%08lX", hr);
        Release();
        return false;
    }

    LUID luid = {};
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapterDesc;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) &&
        SUCCEEDED(dxgiDevice->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetDesc(&adapterDesc))) {
        luid = adapterDesc.AdapterLuid;
    }

    m_width = width;
    m_height = height;
    m_format = format;
    ++m_generation;
    PublishResources(m_handles, width, height, format, luid, api, this);
    return true;
}

bool D3D11FrameRing::Claim(LONG64 frame) const {
    return ClaimSlot(frame, m_hostFence->GetCompletedValue());
}

void D3D11FrameRing::Publish(ID3D11DeviceContext4* context, LONG64 frame, LONGLONG presentQpc,
                             bool flush) {
    context->Signal(m_frameFence.Get(), UINT64(frame));
    if (flush) {
        context->Flush();
    }
    FramePublished(frame, presentQpc);
}

}  // namespace gazeora::hook
