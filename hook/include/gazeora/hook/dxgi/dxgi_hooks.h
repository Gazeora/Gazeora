// IDXGISwapChain::Present / IDXGISwapChain1::Present1 hooks. Every D3D10/11/12
// game presents through DXGI; frames are handed to the D3D10, D3D11 or D3D12
// backend depending on which device owns the swapchain.
#pragma once

namespace gazeora::hook::dxgi {

bool Install();

}  // namespace gazeora::hook::dxgi
