// IDirect3DDevice8::Present and Reset hooks.
#pragma once

namespace gazeora::hook::d3d8 {

// Hooks right away when the game has d3d8.dll loaded. When the host launched the
// game, it instead waits for the game's first Direct3DCreate8.
bool Install();

}  // namespace gazeora::hook::d3d8
