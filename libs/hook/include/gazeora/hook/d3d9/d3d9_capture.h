#pragma once

#include <d3d9.h>
#include <wrl/client.h>

#include "gazeora/hook/core/pixel_convert.h"
#include "gazeora/hook/d3d11/d3d11_bridge.h"

namespace gazeora::hook {

// Reads D3D9 back buffers to the CPU and uploads them to a D3D11 bridge. D3D9
// devices (other than D3D9Ex) can't share resources with other devices, so this is
// the one path that works for every game. Copies are asynchronous: each frame is
// copied into a system-memory surface with an event query behind it, and uploaded
// on a later present once the query says the copy finished.
class D3D9Capture {
public:
    void Reset();
    IDirect3DDevice9* Device() const { return m_device.Get(); }
    void Capture(IDirect3DDevice9* device, IDirect3DSurface9* backBuffer, LONGLONG presentQpc);

private:
    static constexpr UINT kCopies = 3;

    struct Copy {
        Microsoft::WRL::ComPtr<IDirect3DSurface9> surface;  // D3DPOOL_SYSTEMMEM
        Microsoft::WRL::ComPtr<IDirect3DQuery9> query;      // null if unsupported
        LONGLONG presentQpc = 0;
        bool pending = false;
    };

    bool CreateCopies(const D3DSURFACE_DESC& backBuffer);
    void ReleaseCopies();
    void UploadFinishedCopies();
    void Upload(Copy& copy);

    Microsoft::WRL::ComPtr<IDirect3DDevice9> m_device;
    Copy m_copies[kCopies];
    UINT m_nextCopy = 0;  // round-robin, so it's also the oldest pending copy
    Microsoft::WRL::ComPtr<IDirect3DSurface9> m_resolved;  // MSAA back buffers resolve here
    UINT m_width = 0;
    UINT m_height = 0;
    D3DFORMAT m_format = D3DFMT_UNKNOWN;
    PixelLayout m_layout = PixelLayout::BGRA8;
    D3D11Bridge m_bridge;
    bool m_reportedFormat = false;
};

}  // namespace gazeora::hook
