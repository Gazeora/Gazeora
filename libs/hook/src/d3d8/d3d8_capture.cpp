// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/d3d8/d3d8_capture.h"

#include "gazeora/hook/core/log.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook {

void D3D8Capture::Reset() {
    m_copy.Reset();
    m_copyDesc = {};
    m_bridge.Reset();
    m_device.Reset();
    m_reportedUnsupported = false;
}

void D3D8Capture::Capture(d3d8::Device8* device, LONGLONG presentQpc) {
    if (m_device.Get() != device) {
        Reset();
        m_device = device;
    }

    ComPtr<d3d8::Surface8> backBuffer;
    d3d8::SurfaceDesc bb;
    if (FAILED(d3d8::GetBackBuffer(device, 0, &backBuffer)) ||
        FAILED(d3d8::GetDesc(backBuffer.Get(), &bb))) {
        return;
    }
    PixelLayout layout;
    bool supportedFormat = LayoutForD3DFormat(bb.Format, layout);
    if (!supportedFormat || bb.MultiSampleType != D3DMULTISAMPLE_NONE) {
        if (!m_reportedUnsupported) {
            m_reportedUnsupported = true;
            if (supportedFormat) {
                SetStatus("D3D8 multisampled back buffers are not supported");
            } else {
                SetStatus("D3D8 back buffer format %d is not supported", bb.Format);
            }
        }
        return;
    }

    if (!m_copy || bb.Width != m_copyDesc.Width || bb.Height != m_copyDesc.Height ||
        bb.Format != m_copyDesc.Format) {
        m_copy.Reset();
        HRESULT hr = d3d8::CreateImageSurface(device, bb.Width, bb.Height, bb.Format, &m_copy);
        if (FAILED(hr)) {
            SetStatus("D3D8 readback surface %ux%u fmt %d failed: 0x%08lX", bb.Width, bb.Height,
                      bb.Format, hr);
            return;
        }
        m_copyDesc = bb;
    }
    if (!m_bridge.Configure(nullptr, bb.Width, bb.Height, OutputFormat(layout), kApiD3D8) ||
        FAILED(d3d8::CopyRects(device, backBuffer.Get(), m_copy.Get()))) {
        return;
    }
    D3DLOCKED_RECT locked;
    if (SUCCEEDED(d3d8::LockRect(m_copy.Get(), &locked, D3DLOCK_READONLY))) {
        m_bridge.UploadFrame(static_cast<const uint8_t*>(locked.pBits), UINT(locked.Pitch), layout,
                             presentQpc);
        d3d8::UnlockRect(m_copy.Get());
    }
}

}  // namespace gazeora::hook
