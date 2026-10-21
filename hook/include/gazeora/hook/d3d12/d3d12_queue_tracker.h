// A D3D12 swapchain doesn't expose the command queue it presents on, but our copy
// must run on that queue. We hook ID3D12CommandQueue::ExecuteCommandLists to learn
// every queue the game uses, then find the one the swapchain object points to.
#pragma once

#include <d3d12.h>
#include <dxgi.h>

namespace gazeora::hook::d3d12 {

// Hooks ExecuteCommandLists. Needs a D3D12 device to find it, so it only installs
// right away when the game has d3d12.dll loaded (D3D11 games never get a D3D12 device
// created inside them). When the host launched the game, it instead waits for the
// game's first D3D12CreateDevice.
bool InstallQueueTracker();

// The command queue `swap` presents on, or nullptr if the game hasn't submitted
// work on it since the tracker was installed. Not AddRef'd.
ID3D12CommandQueue* FindSwapChainQueue(IDXGISwapChain* swap);

}  // namespace gazeora::hook::d3d12
