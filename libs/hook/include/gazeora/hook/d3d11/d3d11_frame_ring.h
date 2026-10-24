#pragma once

#include <d3d11_4.h>
#include <wrl/client.h>

#include <cstdint>

#include "gazeora/hook/core/shared_channel.h"

namespace gazeora::hook {

// The kFrameSlots shared textures and two shared fences of the frame transport
// (common/shared.h), created on a D3D11 device. D3D11 games use it on their own
// device; every other API except D3D12 uses it through D3D11Bridge.
class D3D11FrameRing {
public:
    D3D11FrameRing() = default;
    ~D3D11FrameRing() { Release(); }
    D3D11FrameRing(const D3D11FrameRing&) = delete;  // owns NT handles
    D3D11FrameRing& operator=(const D3D11FrameRing&) = delete;

    void Release();

    // True if the ring exists with this size and format, with depth slots or without, and
    // the host is reading it.
    bool Matches(UINT width, UINT height, DXGI_FORMAT format, bool depth = false) const;

    // Creates the textures and fences on `device` and publishes them to the host; with
    // `depth`, the depth slots too (shared.h).
    bool Create(ID3D11Device5* device, UINT width, UINT height, DXGI_FORMAT format, GraphicsApi api,
                bool depth = false);

    // Announces that `frame` is about to be written into Slot(frame). False means skip it.
    bool Claim(LONG64 frame) const;

    ID3D11Texture2D* Slot(LONG64 frame) const { return m_slots[frame % kFrameSlots].Get(); }
    // nullptr without depth slots.
    ID3D11Texture2D* DepthSlot(LONG64 frame) const {
        return m_depthSlots[frame % kFrameSlots].Get();
    }

    // NT handles owned by the ring, for APIs that import the slots and the frame fence
    // and write them directly (OpenGL, Vulkan).
    HANDLE SlotHandle(UINT index) const { return m_handles.slots[index]; }
    HANDLE FrameFenceHandle() const { return m_handles.frameFence; }

    // Changes every time Create() makes new resources, so importers know to re-import.
    uint32_t Generation() const { return m_generation; }

    // Queues the frame fence signal for `frame` on `context` (after the commands that
    // wrote the slot) and hands the frame to the host. With `flush`, also submits it;
    // otherwise the caller relies on the game's Present to do so. Importers that
    // signal the frame fence themselves call FramePublished() directly instead.
    void Publish(ID3D11DeviceContext4* context, LONG64 frame, LONGLONG presentQpc, bool flush);

    UINT Width() const { return m_width; }
    UINT Height() const { return m_height; }
    DXGI_FORMAT Format() const { return m_format; }

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_slots[kFrameSlots];
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthSlots[kFrameSlots];
    Microsoft::WRL::ComPtr<ID3D11Fence> m_frameFence;
    Microsoft::WRL::ComPtr<ID3D11Fence> m_hostFence;
    ExportedHandles m_handles;
    UINT m_width = 0;
    UINT m_height = 0;
    DXGI_FORMAT m_format = DXGI_FORMAT_UNKNOWN;
    uint32_t m_generation = 0;
};

}  // namespace gazeora::hook
