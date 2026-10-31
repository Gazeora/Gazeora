// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/d3d9/d3d9_capture.h"

#include "gazeora/hook/core/log.h"

namespace gazeora::hook {

void D3D9Capture::Reset() {
    ReleaseCopies();
    m_bridge.Reset();
    m_device.Reset();
    m_reportedFormat = false;
}

void D3D9Capture::ReleaseCopies() {
    for (Copy& copy : m_copies) copy = Copy();
    m_resolved.Reset();
    m_nextCopy = 0;
    m_width = m_height = 0;
    m_format = D3DFMT_UNKNOWN;
}

bool D3D9Capture::CreateCopies(const D3DSURFACE_DESC& bb) {
    ReleaseCopies();
    HRESULT hr = S_OK;
    for (Copy& copy : m_copies) {
        hr = m_device->CreateOffscreenPlainSurface(bb.Width, bb.Height, bb.Format,
                                                   D3DPOOL_SYSTEMMEM, &copy.surface, nullptr);
        if (FAILED(hr)) {
            break;
        }
        m_device->CreateQuery(D3DQUERYTYPE_EVENT, &copy.query);
    }
    if (SUCCEEDED(hr) && bb.MultiSampleType != D3DMULTISAMPLE_NONE) {
        hr = m_device->CreateRenderTarget(bb.Width, bb.Height, bb.Format, D3DMULTISAMPLE_NONE, 0,
                                          FALSE, &m_resolved, nullptr);
    }
    if (FAILED(hr)) {
        SetStatus("D3D9 readback surfaces %ux%u fmt %d failed: 0x%08lX", bb.Width, bb.Height,
                  bb.Format, hr);
        ReleaseCopies();
        return false;
    }
    m_width = bb.Width;
    m_height = bb.Height;
    m_format = bb.Format;
    return true;
}

void D3D9Capture::Upload(Copy& copy) {
    copy.pending = false;
    if (!m_bridge.Configure(nullptr, m_width, m_height, OutputFormat(m_layout), kApiD3D9)) {
        return;
    }
    D3DLOCKED_RECT locked;
    if (FAILED(copy.surface->LockRect(&locked, nullptr, D3DLOCK_READONLY))) {
        return;
    }
    m_bridge.UploadFrame(static_cast<const uint8_t*>(locked.pBits), UINT(locked.Pitch), m_layout,
                         copy.presentQpc);
    copy.surface->UnlockRect();
}

void D3D9Capture::UploadFinishedCopies() {
    for (UINT i = 0; i < kCopies; ++i) {
        Copy& copy = m_copies[(m_nextCopy + i) % kCopies];
        if (!copy.pending) {
            continue;
        }
        if (copy.query && copy.query->GetData(nullptr, 0, 0) != S_OK) {
            break;  // later copies can't have finished either
        }
        Upload(copy);
    }
}

void D3D9Capture::Capture(IDirect3DDevice9* device, IDirect3DSurface9* backBuffer,
                          LONGLONG presentQpc) {
    if (m_device.Get() != device) {
        Reset();
        m_device = device;
    }

    D3DSURFACE_DESC bb;
    if (FAILED(backBuffer->GetDesc(&bb))) {
        return;
    }
    if (!LayoutForD3DFormat(bb.Format, m_layout)) {
        if (!m_reportedFormat) {
            m_reportedFormat = true;
            SetStatus("D3D9 back buffer format %d is not supported", bb.Format);
        }
        return;
    }
    if (bb.Width != m_width || bb.Height != m_height || bb.Format != m_format) {
        if (!CreateCopies(bb)) {
            return;
        }
    }

    UploadFinishedCopies();

    Copy& copy = m_copies[m_nextCopy];
    if (copy.pending) {
        CountSkippedFrame();  // the GPU is more than kCopies frames behind
        return;
    }
    HRESULT hr;
    if (m_resolved) {
        hr = m_device->StretchRect(backBuffer, nullptr, m_resolved.Get(), nullptr, D3DTEXF_NONE);
        if (SUCCEEDED(hr)) hr = m_device->GetRenderTargetData(m_resolved.Get(), copy.surface.Get());
    } else {
        hr = m_device->GetRenderTargetData(backBuffer, copy.surface.Get());
    }
    if (FAILED(hr)) {
        return;  // e.g. device lost
    }
    if (copy.query) {
        copy.query->Issue(D3DISSUE_END);
    }
    copy.presentQpc = presentQpc;
    copy.pending = true;
    m_nextCopy = (m_nextCopy + 1) % kCopies;
}

}  // namespace gazeora::hook
