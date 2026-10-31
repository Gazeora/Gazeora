// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// IDirect3DDevice9::Present / PresentEx and IDirect3DSwapChain9::Present hooks, plus
// Reset / ResetEx so capture resources are released before the game resets its device.
#pragma once

namespace gazeora::hook::d3d9 {

// Hooks right away when the game has d3d9.dll loaded. When the host launched the
// game, it instead waits for the game's first Direct3DCreate9(Ex).
bool Install();

}  // namespace gazeora::hook::d3d9
