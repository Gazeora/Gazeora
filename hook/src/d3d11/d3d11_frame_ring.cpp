#include "gazeora/hook/d3d11/d3d11_frame_ring.h"

#include "gazeora/hook/core/log.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook {

void D3D11FrameRing::Release() {
    m_handles.Close();
    for (auto& slot : m_slots) slot.Reset();
    m_frameFence.Reset();
    m_hostFence.Reset();
    m_width = m_height = 0;
    m_format = DXGI_FORMAT_UNKNOWN;
}

bool D3D11FrameRing::Matches(UINT width, UINT height, DXGI_FORMAT format) const {
    return m_frameFence && width == m_width && height == m_height && format == m_format &&
           IsPublisher(this);
}

bool D3D11FrameRing::Create(ID3D11Device5* device, UINT width, UINT height, DXGI_FORMAT format,
                            GraphicsApi api) {
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

    for (UINT i = 0; i < kFrameSlots; ++i) {
        ComPtr<IDXGIResource1> resource;
        HRESULT hr = device->CreateTexture2D(&desc, nullptr, &m_slots[i]);
        if (SUCCEEDED(hr)) hr = m_slots[i].As(&resource);
        if (SUCCEEDED(hr)) {
            hr = resource->CreateSharedHandle(
                nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
                &m_handles.slots[i]);
        }
        if (FAILED(hr)) {
            SetStatus("D3D11 shared texture %ux%u fmt %d failed: 0x%08lX", width, height, format,
                      hr);
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
