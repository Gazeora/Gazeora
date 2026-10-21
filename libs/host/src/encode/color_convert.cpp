#include "gazeora/host/encode/color_convert.h"

namespace gazeora {

void BgraToNv12(const uint8_t* bgra, uint32_t stride, uint32_t width, uint32_t height,
                std::vector<uint8_t>& nv12) {
    nv12.resize(size_t(width) * height * 3 / 2);
    uint8_t* yPlane = nv12.data();
    uint8_t* uvPlane = nv12.data() + size_t(width) * height;
    // BT.709 in fixed point (x256), studio range: Y 16-235, U and V 16-240.
    for (uint32_t y = 0; y < height; y += 2) {
        const uint8_t* row0 = bgra + size_t(y) * stride * 4;
        const uint8_t* row1 = row0 + size_t(stride) * 4;
        uint8_t* y0 = yPlane + size_t(y) * width;
        uint8_t* y1 = y0 + width;
        uint8_t* uv = uvPlane + size_t(y / 2) * width;
        for (uint32_t x = 0; x < width; x += 2) {
            int r = 0, g = 0, b = 0;
            for (int dy = 0; dy < 2; ++dy) {
                const uint8_t* p = (dy ? row1 : row0) + size_t(x) * 4;
                uint8_t* out = (dy ? y1 : y0) + x;
                for (int dx = 0; dx < 2; ++dx, p += 4) {
                    out[dx] = uint8_t(16 + ((47 * p[2] + 157 * p[1] + 16 * p[0] + 128) >> 8));
                    r += p[2], g += p[1], b += p[0];
                }
            }
            // Chroma from the average of the 2x2 block; r, g, b are four times the mean.
            uv[x] = uint8_t(128 + ((-26 * r - 87 * g + 112 * b + 512) >> 10));
            uv[x + 1] = uint8_t(128 + ((112 * r - 102 * g - 10 * b + 512) >> 10));
        }
    }
}

}  // namespace gazeora
