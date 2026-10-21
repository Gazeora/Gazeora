#pragma once

#include <d3d11_4.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

#include "gazeora/hook/core/pixel_convert.h"
#include "gazeora/hook/d3d11/d3d11_frame_ring.h"

namespace gazeora::hook {

// A private D3D11 device inside the game that owns the frame ring, for APIs that can't
// create NT-shared textures and fences themselves (D3D8, D3D9, D3D10, OpenGL, Vulkan).
// They get their frames into Ring() either on the GPU (D3D10, OpenGL interop, Vulkan)
// or by uploading CPU readbacks (D3D8, D3D9, OpenGL fallback).
class D3D11Bridge {
public:
    void Reset();

    // Makes sure the device exists on `adapter` (the default adapter if null) and the
    // ring has this size and format. Cheap when nothing changed.
    bool Configure(const LUID* adapter, UINT width, UINT height, DXGI_FORMAT format,
                   GraphicsApi api);

    ID3D11Device5* Device() const { return m_device.Get(); }
    ID3D11DeviceContext4* Context() const { return m_context.Get(); }
    D3D11FrameRing& Ring() { return m_ring; }

    // Hands a frame written into Ring().Slot(frame) on Context() to the host.
    void Publish(LONG64 frame, LONGLONG presentQpc);

    // Uploads one CPU frame of the configured size, with pixels in `layout`
    // (OutputFormat(layout) must be the configured format), and publishes it.
    // `bottomUp` means rows are stored bottom to top, as glReadPixels returns them.
    void UploadFrame(const uint8_t* data, UINT pitch, PixelLayout layout, LONGLONG presentQpc,
                     bool bottomUp = false);

private:
    bool CreateDevice(const LUID* adapter);

    Microsoft::WRL::ComPtr<ID3D11Device5> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> m_context;
    LUID m_adapter = {};
    bool m_defaultAdapter = false;
    D3D11FrameRing m_ring;
    std::vector<uint8_t> m_converted;
};

}  // namespace gazeora::hook
