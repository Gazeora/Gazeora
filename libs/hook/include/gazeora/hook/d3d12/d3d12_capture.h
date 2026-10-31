// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <d3d12.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "gazeora/hook/core/shared_channel.h"

namespace gazeora::hook {

// Copies the current back buffer of a D3D12 swapchain into the shared frame ring, and the
// game's depth (d3d12_depth.h) into the depth slots when the host wants it, using a command
// list submitted on the game's own present queue.
class D3D12Capture {
public:
    void Reset();
    void Capture(IDXGISwapChain* swap, ID3D12Device* device, LONGLONG presentQpc);

private:
    bool AttachQueue(IDXGISwapChain* swap);
    bool CreateCommandObjects();
    bool CreateResources(const D3D12_RESOURCE_DESC& backBuffer, bool depth);
    void ReleaseResources();
    void WaitForGpu();

    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_allocators[kFrameSlots];
    UINT64 m_allocatorFrame[kFrameSlots] = {};
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_list;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_slots[kFrameSlots];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthSlots[kFrameSlots];  // with depth only
    // The buffer each slot's depth was copied from, kept until that copy has run.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthSources[kFrameSlots];
    Microsoft::WRL::ComPtr<ID3D12Fence> m_frameFence;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_hostFence;
    ExportedHandles m_handles;
    UINT64 m_lastSubmitted = 0;
    UINT64 m_width = 0;
    UINT m_height = 0;
    DXGI_FORMAT m_format = DXGI_FORMAT_UNKNOWN;
    bool m_reportedNoQueue = false;
};

}  // namespace gazeora::hook
