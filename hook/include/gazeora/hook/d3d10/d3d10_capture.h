#pragma once

#include <d3d10.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "gazeora/hook/d3d11/d3d11_bridge.h"

namespace gazeora::hook {

// Copies the back buffer of a D3D10 swapchain into the frame ring of a D3D11 bridge
// on the same GPU. D3D10 can't open NT-shared textures or fences (and D3D10.0 can't
// use keyed mutexes), so each frame is copied on the game's device into a texture
// shared the legacy way, with an event query behind it. Once the query says the copy
// finished, on a later present, the bridge copies that texture into a ring slot.
class D3D10Capture {
public:
    void Reset();
    void Capture(IDXGISwapChain* swap, ID3D10Device* device, LONGLONG presentQpc);

private:
    static constexpr UINT kCopies = 3;

    struct Copy {
        Microsoft::WRL::ComPtr<ID3D10Texture2D> gameTexture;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> bridgeTexture;  // same texture, on the bridge
        Microsoft::WRL::ComPtr<ID3D10Query> query;
        LONGLONG presentQpc = 0;
        bool pending = false;
    };

    bool CreateCopies(UINT width, UINT height, DXGI_FORMAT format);
    void ReleaseCopies();
    void PublishFinishedCopies();

    Microsoft::WRL::ComPtr<ID3D10Device> m_device;
    LUID m_adapterLuid = {};
    D3D11Bridge m_bridge;
    Copy m_copies[kCopies];
    UINT m_nextCopy = 0;  // round-robin, so it's also the oldest pending copy
    UINT m_width = 0;
    UINT m_height = 0;
    DXGI_FORMAT m_format = DXGI_FORMAT_UNKNOWN;
};

}  // namespace gazeora::hook
