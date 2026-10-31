// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "gazeora/common/shared.h"

namespace gazeora::hook {

namespace d3d11 {

// Hooks the device context calls that tell which depth buffer the game draws with and when it
// clears it: draws, render target binds and depth clears. Every D3D11 context runs the same
// code, so any device's context finds them. They only do work while the host wants depth.
bool InstallDepthHooks(ID3D11Device* device);

}  // namespace d3d11

// Copies the game's main depth buffer (DepthPicker) into a depth slot at Present, for 3D from
// depth. A game that clears it partway through a frame, to draw a first-person weapon on its
// own say, has it copied before that clear too, and the copy with the most draws behind it is
// the one kept: the scene's.
class D3D11DepthCapture {
public:
    void Reset();

    // On the game's immediate context, at Present: writes this frame's depth into `slot`
    // (RGBA8, the back buffer's size), as kDepthFloat. False if there's none to write.
    bool Capture(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* slot,
                 UINT width, UINT height, UINT& depthWidth, UINT& depthHeight);

private:
    bool CreateShaders(ID3D11Device* device);
    // Draws the depth copy into `slot` as 32-bit words, leaving the game's state as it was.
    bool Convert(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* slot,
                 ID3D11ShaderResourceView* depth, UINT width, UINT height);

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    bool m_shaderFailed = false;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    // Render target views of the slots last written, with the slot each is for.
    ID3D11Texture2D* m_targetTextures[kFrameSlots] = {};  // identity only
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_targets[kFrameSlots];
    UINT m_nextTarget = 0;  // the one a new slot's view replaces
};

}  // namespace gazeora::hook
