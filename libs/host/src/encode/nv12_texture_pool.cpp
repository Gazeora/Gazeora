#include "gazeora/host/encode/nv12_texture_pool.h"

namespace gazeora {

bool Nv12TexturePool::Supported(ID3D11Device* device) {
    UINT support = 0;
    return device && SUCCEEDED(device->CheckFormatSupport(DXGI_FORMAT_NV12, &support)) &&
           (support & D3D11_FORMAT_SUPPORT_RENDER_TARGET);
}

const Nv12TexturePool::Slot* Nv12TexturePool::Next(ID3D11Device* device, UINT width,
                                                   UINT height) {
    if (m_device.Get() != device || m_width != width || m_height != height) {
        // Textures still out with the encoder live on through its references.
        m_slots = {};
        m_device = device;
        m_width = width;
        m_height = height;
    }
    Slot& slot = m_slots[m_next];
    if (!slot.texture && !Create(slot)) {
        slot = {};
        return nullptr;
    }
    m_next = (m_next + 1) % kSize;
    return &slot;
}

bool Nv12TexturePool::Create(Slot& slot) const {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    // Each plane is drawn through a view of its own format: R8 is the luma, R8G8 the chroma.
    D3D11_RENDER_TARGET_VIEW_DESC luma = {};
    luma.Format = DXGI_FORMAT_R8_UNORM;
    luma.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    D3D11_RENDER_TARGET_VIEW_DESC chroma = luma;
    chroma.Format = DXGI_FORMAT_R8G8_UNORM;
    return SUCCEEDED(m_device->CreateTexture2D(&desc, nullptr, &slot.texture)) &&
           SUCCEEDED(m_device->CreateRenderTargetView(slot.texture.Get(), &luma, &slot.luma)) &&
           SUCCEEDED(m_device->CreateRenderTargetView(slot.texture.Get(), &chroma, &slot.chroma));
}

}  // namespace gazeora
