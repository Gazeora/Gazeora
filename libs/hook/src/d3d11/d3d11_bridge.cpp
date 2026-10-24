#include "gazeora/hook/d3d11/d3d11_bridge.h"

#include <dxgi1_2.h>

#include "gazeora/hook/core/log.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook {

namespace {

bool SameLuid(const LUID& a, const LUID& b) {
    return a.LowPart == b.LowPart && a.HighPart == b.HighPart;
}

ComPtr<IDXGIAdapter1> FindAdapter(const LUID& luid) {
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        return nullptr;
    }
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        if (SUCCEEDED(adapter->GetDesc1(&desc)) && SameLuid(desc.AdapterLuid, luid)) {
            return adapter;
        }
    }
    return nullptr;
}

}  // namespace

void D3D11Bridge::Reset() {
    m_ring.Release();
    m_context.Reset();
    m_device.Reset();
    m_converted.clear();
    m_converted.shrink_to_fit();
}

bool D3D11Bridge::CreateDevice(const LUID* adapter) {
    Reset();

    ComPtr<IDXGIAdapter1> dxgiAdapter;
    if (adapter) {
        dxgiAdapter = FindAdapter(*adapter);
        if (!dxgiAdapter) {
            SetStatus("no DXGI adapter matches the game's GPU");
            return false;
        }
    }
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    HRESULT hr = D3D11CreateDevice(dxgiAdapter.Get(),
                                   dxgiAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                                   nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                   D3D11_SDK_VERSION, &device, nullptr, &context);
    if (FAILED(hr)) {
        SetStatus("D3D11CreateDevice for the capture bridge failed: 0x%08lX", hr);
        return false;
    }
    if (FAILED(device.As(&m_device)) || FAILED(context.As(&m_context))) {
        SetStatus("capture bridge needs D3D11.4 (Windows 10 1703+)");
        Reset();
        return false;
    }
    m_defaultAdapter = adapter == nullptr;
    m_adapter = adapter ? *adapter : LUID{};
    return true;
}

bool D3D11Bridge::Configure(const LUID* adapter, UINT width, UINT height, DXGI_FORMAT format,
                            GraphicsApi api) {
    bool sameAdapter =
        adapter ? !m_defaultAdapter && SameLuid(*adapter, m_adapter) : m_defaultAdapter;
    if ((!m_device || !sameAdapter) && !CreateDevice(adapter)) {
        return false;
    }
    return m_ring.Matches(width, height, format) ||
           m_ring.Create(m_device.Get(), width, height, format, api);
}

void D3D11Bridge::Publish(LONG64 frame, LONGLONG presentQpc) {
    m_ring.Publish(m_context.Get(), frame, presentQpc, true);
}

void D3D11Bridge::UploadFrame(const uint8_t* data, UINT pitch, PixelLayout layout,
                              LONGLONG presentQpc, bool bottomUp) {
    LONG64 frame = NextFrameNumber();
    if (!m_ring.Claim(frame)) {
        return;
    }
    UINT width = m_ring.Width();
    UINT height = m_ring.Height();
    if (NeedsConversion(layout) || bottomUp) {
        UINT outPitch = width * 4;
        m_converted.resize(size_t(outPitch) * height);
        for (UINT y = 0; y < height; ++y) {
            UINT srcRow = bottomUp ? height - 1 - y : y;
            ConvertRow(layout, data + size_t(srcRow) * pitch,
                       m_converted.data() + size_t(y) * outPitch, width);
        }
        data = m_converted.data();
        pitch = outPitch;
    }
    m_context->UpdateSubresource(m_ring.Slot(frame), 0, nullptr, data, pitch, 0);
    Publish(frame, presentQpc);
}

}  // namespace gazeora::hook
