// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// 3D from depth in D3D12 games. The depth buffer's state at Present is unknown, so it's
// copied where the game's own command lists show it: when a resource barrier takes the main
// depth buffer (DepthPicker) out of DEPTH_WRITE, the scene is drawn, and a copy into a buffer
// of ours is recorded right after that barrier, in the state the barrier leaves it in. A game
// that never takes it out of DEPTH_WRITE has it copied before it's cleared for the next frame
// instead, a frame late. At Present, D3D12Capture copies that buffer into the depth slot.
//
// Binds name depth buffers by descriptor, known from the views the game creates. Views made
// before the hook came in (a game already running when attached to) stay unknown; the
// barriers still show the buffers, but such a game without them gets no depth.
#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include "gazeora/common/shared.h"

namespace gazeora::hook::d3d12 {

// Hooks CreateDepthStencilView, to know the buffer behind a depth descriptor, and the command
// lists' render target binds, draws, depth clears and resource barriers. `enable` turns them
// on right away, for when the other hooks already are. They only do work while the host wants
// depth.
bool InstallDepthHooks(ID3D12Device* device, bool enable);

// The latest depth the game's command lists copied out.
struct DepthCopy {
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};  // of the depth values in `buffer`
    DepthEncoding encoding = kDepthNone;
};

// At Present: the buffer this frame's depth is copied into, false if there's none yet. Also
// ends the frame for picking the depth buffer to copy next. Keep `copy.buffer` until the GPU
// is done reading it.
bool TakeDepthCopy(ID3D12Device* device, UINT backBufferWidth, UINT backBufferHeight,
                   DepthCopy& copy);

}  // namespace gazeora::hook::d3d12
