#pragma once

#include <d3d11_4.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "gazeora/hook/d3d11/d3d11_depth.h"
#include "gazeora/hook/d3d11/d3d11_frame_ring.h"

namespace gazeora::hook {

// Copies the back buffer of a D3D11 swapchain into the shared frame ring, and the depth buffer
// too when the host wants it.
class D3D11Capture {
public:
    void Reset();
    void Capture(IDXGISwapChain* swap, ID3D11Device* device, LONGLONG presentQpc);

private:
    bool AttachDevice(ID3D11Device* device);
    bool IsCurrentDevice(ID3D11Device* device) const;

    Microsoft::WRL::ComPtr<ID3D11Device5> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> m_context;
    D3D11FrameRing m_ring;
    D3D11DepthCapture m_depth;
};

}  // namespace gazeora::hook
