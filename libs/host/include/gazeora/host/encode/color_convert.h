#pragma once

#include <cstdint>
#include <vector>

namespace gazeora {

// Converts BGRA8 to NV12 (BT.709, limited range), the format video encoders take. Width
// and height must be even; `stride` is the source row length in pixels. On the CPU; live
// frames are converted on the GPU instead (TextureReader::ReadNv12), this is for pictures
// that start out in memory, such as the idle logo.
void BgraToNv12(const uint8_t* bgra, uint32_t stride, uint32_t width, uint32_t height,
                std::vector<uint8_t>& nv12);

}  // namespace gazeora
