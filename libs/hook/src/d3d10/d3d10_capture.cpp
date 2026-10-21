#include "gazeora/hook/d3d10/d3d10_capture.h"

#include "gazeora/hook/core/log.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook {

void D3D10Capture::Reset() {
    ReleaseCopies();
    m_bridge.Reset();
    m_device.Reset();
}

void D3D10Capture::ReleaseCopies() {
    for (Copy& copy : m_copies) copy = Copy();
    m_nextCopy = 0;
    m_width = m_height = 0;
    m_format = DXGI_FORMAT_UNKNOWN;
}

// Created on the game's device and opened on the bridge (the other way around fails
// on D3D10.0 devices).
bool D3D10Capture::CreateCopies(UINT width, UINT height, DXGI_FORMAT format) {
    ReleaseCopies();

    D3D10_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D10_USAGE_DEFAULT;
    desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D10_RESOURCE_MISC_SHARED;
    D3D10_QUERY_DESC queryDesc = {D3D10_QUERY_EVENT, 0};

    HRESULT hr = S_OK;
    for (Copy& copy : m_copies) {
        ComPtr<IDXGIResource> resource;
        HANDLE handle = nullptr;
        hr = m_device->CreateTexture2D(&desc, nullptr, &copy.gameTexture);
        if (SUCCEEDED(hr)) hr = copy.gameTexture.As(&resource);
        if (SUCCEEDED(hr)) hr = resource->GetSharedHandle(&handle);
        if (SUCCEEDED(hr)) {
            hr = m_bridge.Device()->OpenSharedResource(handle, IID_PPV_ARGS(&copy.bridgeTexture));
        }
        if (SUCCEEDED(hr)) hr = m_device->CreateQuery(&queryDesc, &copy.query);
        if (FAILED(hr)) {
            break;
        }
    }
    if (FAILED(hr)) {
        SetStatus("D3D10 shared texture %ux%u fmt %d failed: 0x%08lX", width, height, format, hr);
        ReleaseCopies();
        return false;
    }
    m_width = width;
    m_height = height;
    m_format = format;
    return true;
}

void D3D10Capture::PublishFinishedCopies() {
    D3D11FrameRing& ring = m_bridge.Ring();
    for (UINT i = 0; i < kCopies; ++i) {
        Copy& copy = m_copies[(m_nextCopy + i) % kCopies];
        if (!copy.pending) {
            continue;
        }
        if (copy.query->GetData(nullptr, 0, D3D10_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
            break;  // later copies can't have finished either
        }
        copy.pending = false;
        LONG64 frame = NextFrameNumber();
        if (ring.Claim(frame)) {
            m_bridge.Context()->CopyResource(ring.Slot(frame), copy.bridgeTexture.Get());
            m_bridge.Publish(frame, copy.presentQpc);
        }
    }
}

void D3D10Capture::Capture(IDXGISwapChain* swap, ID3D10Device* device, LONGLONG presentQpc) {
    if (m_device.Get() != device) {
        Reset();
        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        DXGI_ADAPTER_DESC adapterDesc;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) ||
            FAILED(dxgiDevice->GetAdapter(&adapter)) || FAILED(adapter->GetDesc(&adapterDesc))) {
            SetStatus("D3D10 game, but its DXGI adapter is unavailable");
            return;
        }
        m_device = device;
        m_adapterLuid = adapterDesc.AdapterLuid;
    }

    ComPtr<ID3D10Texture2D> backBuffer;
    if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        return;
    }
    D3D10_TEXTURE2D_DESC bb;
    backBuffer->GetDesc(&bb);
    if (!m_bridge.Configure(&m_adapterLuid, bb.Width, bb.Height, bb.Format, kApiD3D10)) {
        return;
    }
    if ((bb.Width != m_width || bb.Height != m_height || bb.Format != m_format) &&
        !CreateCopies(bb.Width, bb.Height, bb.Format)) {
        return;
    }

    PublishFinishedCopies();

    Copy& copy = m_copies[m_nextCopy];
    if (copy.pending) {
        CountSkippedFrame();  // the GPU is more than kCopies frames behind
        return;
    }
    if (bb.SampleDesc.Count > 1) {
        m_device->ResolveSubresource(copy.gameTexture.Get(), 0, backBuffer.Get(), 0, bb.Format);
    } else {
        m_device->CopyResource(copy.gameTexture.Get(), backBuffer.Get());
    }
    copy.query->End();  // flushed by the game's Present right after this
    copy.presentQpc = presentQpc;
    copy.pending = true;
    m_nextCopy = (m_nextCopy + 1) % kCopies;
}

}  // namespace gazeora::hook
