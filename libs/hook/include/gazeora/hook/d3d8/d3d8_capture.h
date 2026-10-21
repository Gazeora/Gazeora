#pragma once

#include <wrl/client.h>

#include "gazeora/hook/core/pixel_convert.h"
#include "gazeora/hook/d3d11/d3d11_bridge.h"
#include "gazeora/hook/d3d8/d3d8_api.h"

namespace gazeora::hook {

// Reads D3D8 back buffers to the CPU and uploads them to a D3D11 bridge. D3D8
// has no resource sharing and no queries, so the readback is synchronous.
class D3D8Capture {
public:
    void Reset();
    d3d8::Device8* Device() const { return m_device.Get(); }
    void Capture(d3d8::Device8* device, LONGLONG presentQpc);

private:
    Microsoft::WRL::ComPtr<d3d8::Device8> m_device;
    Microsoft::WRL::ComPtr<d3d8::Surface8> m_copy;  // system memory
    d3d8::SurfaceDesc m_copyDesc = {};
    D3D11Bridge m_bridge;
    bool m_reportedUnsupported = false;
};

}  // namespace gazeora::hook
