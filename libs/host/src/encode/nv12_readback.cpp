// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/encode/nv12_readback.h"

#include <cstring>

using Microsoft::WRL::ComPtr;

namespace gazeora {

bool Nv12Readback::Read(ID3D11Texture2D* nv12, uint32_t width, uint32_t height,
                        std::vector<uint8_t>& out, std::wstring& error) {
    D3D11_TEXTURE2D_DESC desc;
    nv12->GetDesc(&desc);
    if (desc.Format != DXGI_FORMAT_NV12 || desc.Width < width || desc.Height < height) {
        error = L"not an NV12 frame of the encoder's size";
        return false;
    }
    ComPtr<ID3D11Device> device;
    nv12->GetDevice(&device);
    if (!PrepareStaging(device.Get(), desc)) {
        error = L"failed to create the NV12 staging texture";
        return false;
    }
    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    context->CopyResource(m_staging.Get(), nv12);
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(m_staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        error = L"failed to map the NV12 staging texture";
        return false;
    }
    // The chroma plane follows the luma plane's full height of rows, at the same pitch.
    const uint8_t* luma = static_cast<const uint8_t*>(mapped.pData);
    const uint8_t* chroma = luma + size_t(mapped.RowPitch) * desc.Height;
    out.resize(size_t(width) * height * 3 / 2);
    uint8_t* to = out.data();
    for (uint32_t y = 0; y < height; ++y, to += width) {
        memcpy(to, luma + size_t(y) * mapped.RowPitch, width);
    }
    for (uint32_t y = 0; y < height / 2; ++y, to += width) {
        memcpy(to, chroma + size_t(y) * mapped.RowPitch, width);
    }
    context->Unmap(m_staging.Get(), 0);
    return true;
}

bool Nv12Readback::PrepareStaging(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& source) {
    if (m_staging && m_device.Get() == device && m_width == source.Width &&
        m_height == source.Height) {
        return true;
    }
    D3D11_TEXTURE2D_DESC desc = source;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    m_staging.Reset();
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &m_staging))) {
        return false;
    }
    m_device = device;
    m_width = source.Width;
    m_height = source.Height;
    return true;
}

}  // namespace gazeora
