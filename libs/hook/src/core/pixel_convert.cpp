#include "gazeora/hook/core/pixel_convert.h"

#include <cstring>

namespace gazeora::hook {

namespace {

// D3DFORMAT values from d3d9types.h (unchanged since D3D8).
constexpr uint32_t kD3DFmtA8R8G8B8 = 21;
constexpr uint32_t kD3DFmtX8R8G8B8 = 22;
constexpr uint32_t kD3DFmtR5G6B5 = 23;
constexpr uint32_t kD3DFmtX1R5G5B5 = 24;
constexpr uint32_t kD3DFmtA1R5G5B5 = 25;
constexpr uint32_t kD3DFmtA2B10G10R10 = 31;
constexpr uint32_t kD3DFmtA8B8G8R8 = 32;
constexpr uint32_t kD3DFmtX8B8G8R8 = 33;
constexpr uint32_t kD3DFmtA2R10G10B10 = 35;

uint8_t Expand5(uint32_t v) { return uint8_t((v << 3) | (v >> 2)); }
uint8_t Expand6(uint32_t v) { return uint8_t((v << 2) | (v >> 4)); }

}  // namespace

DXGI_FORMAT OutputFormat(PixelLayout layout) {
    switch (layout) {
        case PixelLayout::RGBA8:
        case PixelLayout::RGBX8:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case PixelLayout::RGB10A2:
        case PixelLayout::BGR10A2:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        default:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
    }
}

uint32_t SourceBytesPerPixel(PixelLayout layout) {
    switch (layout) {
        case PixelLayout::B5G6R5:
        case PixelLayout::B5G5R5X1:
            return 2;
        default:
            return 4;
    }
}

bool NeedsConversion(PixelLayout layout) {
    return layout != PixelLayout::BGRA8 && layout != PixelLayout::RGBA8 &&
           layout != PixelLayout::RGB10A2;
}

void ConvertRow(PixelLayout layout, const uint8_t* src, uint8_t* dst, uint32_t width) {
    switch (layout) {
        case PixelLayout::BGRA8:
        case PixelLayout::RGBA8:
        case PixelLayout::RGB10A2:
            memcpy(dst, src, size_t(width) * 4);
            break;
        case PixelLayout::BGRX8:
        case PixelLayout::RGBX8:
            for (uint32_t x = 0; x < width; ++x, src += 4, dst += 4) {
                dst[0] = src[0], dst[1] = src[1], dst[2] = src[2], dst[3] = 255;
            }
            break;
        case PixelLayout::BGR10A2:
            for (uint32_t x = 0; x < width; ++x, src += 4, dst += 4) {
                uint32_t v;
                memcpy(&v, src, 4);
                v = (v & 0xC00FFC00) | ((v & 0x3FF) << 20) | ((v >> 20) & 0x3FF);
                memcpy(dst, &v, 4);
            }
            break;
        case PixelLayout::B5G6R5:
            for (uint32_t x = 0; x < width; ++x, src += 2, dst += 4) {
                uint32_t v = uint32_t(src[0]) | (uint32_t(src[1]) << 8);
                dst[0] = Expand5(v & 0x1F);
                dst[1] = Expand6((v >> 5) & 0x3F);
                dst[2] = Expand5(v >> 11);
                dst[3] = 255;
            }
            break;
        case PixelLayout::B5G5R5X1:
            for (uint32_t x = 0; x < width; ++x, src += 2, dst += 4) {
                uint32_t v = uint32_t(src[0]) | (uint32_t(src[1]) << 8);
                dst[0] = Expand5(v & 0x1F);
                dst[1] = Expand5((v >> 5) & 0x1F);
                dst[2] = Expand5((v >> 10) & 0x1F);
                dst[3] = 255;
            }
            break;
    }
}

bool LayoutForD3DFormat(uint32_t d3dFormat, PixelLayout& layout) {
    switch (d3dFormat) {
        case kD3DFmtA8R8G8B8:
            layout = PixelLayout::BGRA8;
            return true;
        case kD3DFmtX8R8G8B8:
            layout = PixelLayout::BGRX8;
            return true;
        case kD3DFmtA8B8G8R8:
            layout = PixelLayout::RGBA8;
            return true;
        case kD3DFmtX8B8G8R8:
            layout = PixelLayout::RGBX8;
            return true;
        case kD3DFmtA2B10G10R10:
            layout = PixelLayout::RGB10A2;
            return true;
        case kD3DFmtA2R10G10B10:
            layout = PixelLayout::BGR10A2;
            return true;
        case kD3DFmtR5G6B5:
            layout = PixelLayout::B5G6R5;
            return true;
        case kD3DFmtX1R5G5B5:
        case kD3DFmtA1R5G5B5:
            layout = PixelLayout::B5G5R5X1;
            return true;
        default:
            return false;
    }
}

}  // namespace gazeora::hook
