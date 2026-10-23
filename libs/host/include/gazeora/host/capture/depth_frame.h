#pragma once

#include <d3d11.h>

#include <cstdint>

#include "gazeora/common/shared.h"

namespace gazeora {

// The game's depth buffer for a captured frame, as the hook sends it (common/shared.h): an
// RGBA8 texture the frame's size, each texel one depth value as a 32-bit word.
struct DepthFrame {
    ID3D11Texture2D* texture = nullptr;
    // The depth buffer's own size; the texture holds as much of it as fits, from the top left.
    uint32_t width = 0;
    uint32_t height = 0;
    DepthEncoding encoding = kDepthNone;
};

}  // namespace gazeora
