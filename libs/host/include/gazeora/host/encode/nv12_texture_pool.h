// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>

namespace gazeora {

// Object pool pattern: NV12 textures to draw frames into, used in turn. The encoder reads a
// frame on its own thread after it's been handed over, so a texture comes round again only
// kSize frames later, long after the encoder is done with it (or has dropped it unread).
class Nv12TexturePool {
public:
    static constexpr size_t kSize = 4;

    // One texture, with a view for drawing each plane.
    struct Slot {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> luma;    // R8, full size
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> chroma;  // R8G8, half size
    };

    // Whether `device` can draw into NV12 textures, which the pool needs.
    static bool Supported(ID3D11Device* device);

    // The next texture, width x height (even) on `device`. Null if it can't be made.
    const Slot* Next(ID3D11Device* device, UINT width, UINT height);

private:
    bool Create(Slot& slot) const;

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    UINT m_width = 0;
    UINT m_height = 0;
    std::array<Slot, kSize> m_slots;
    size_t m_next = 0;
};

}  // namespace gazeora
