#include "gazeora/hook/d3d11/d3d11_capture.h"

#include "gazeora/hook/core/log.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook {

void D3D11Capture::Reset() {
    m_depth.Reset();
    m_ring.Release();
    m_context.Reset();
    m_device.Reset();
}

bool D3D11Capture::IsCurrentDevice(ID3D11Device* device) const {
    if (!m_device) {
        return false;
    }
    ComPtr<IUnknown> a, b;
    device->QueryInterface(IID_PPV_ARGS(&a));
    m_device->QueryInterface(IID_PPV_ARGS(&b));
    return a == b;
}

bool D3D11Capture::AttachDevice(ID3D11Device* device) {
    Reset();
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&m_device)))) {
        SetStatus("D3D11 game, but ID3D11Device5 is unavailable (needs Windows 10 1703+)");
        return false;
    }
    ComPtr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);
    if (FAILED(context.As(&m_context))) {
        SetStatus("D3D11 game, but ID3D11DeviceContext4 is unavailable");
        m_device.Reset();
        return false;
    }
    return true;
}

void D3D11Capture::Capture(IDXGISwapChain* swap, ID3D11Device* device, LONGLONG presentQpc) {
    if (!IsCurrentDevice(device) && !AttachDevice(device)) {
        return;
    }

    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        return;
    }
    D3D11_TEXTURE2D_DESC bb;
    backBuffer->GetDesc(&bb);
    bool depth = CaptureDepth();
    if (!m_ring.Matches(bb.Width, bb.Height, bb.Format, depth) &&
        !m_ring.Create(m_device.Get(), bb.Width, bb.Height, bb.Format, kApiD3D11, depth)) {
        return;
    }

    LONG64 frame = NextFrameNumber();
    if (!m_ring.Claim(frame)) {
        return;
    }
    ID3D11Texture2D* slot = m_ring.Slot(frame);
    if (bb.SampleDesc.Count > 1) {
        m_context->ResolveSubresource(slot, 0, backBuffer.Get(), 0, bb.Format);
    } else {
        m_context->CopyResource(slot, backBuffer.Get());
    }
    UINT depthWidth, depthHeight;
    if (depth && m_depth.Capture(m_device.Get(), m_context.Get(), m_ring.DepthSlot(frame),
                                 bb.Width, bb.Height, depthWidth, depthHeight)) {
        PublishDepth(frame, depthWidth, depthHeight, kDepthFloat);
    }
    // Flushed to the GPU by the game's Present right after this.
    m_ring.Publish(m_context.Get(), frame, presentQpc, false);
}

}  // namespace gazeora::hook
