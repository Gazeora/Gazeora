#include "gazeora/hook/core/pixel_convert.h"

#include <emmintrin.h>

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

// The rows are converted with SSE2 (every x64 CPU has it), 4 or 8 pixels at a time; the
// scalar loops finish the rest of each row. SSE2 only: MinGW GCC doesn't keep the stack
// aligned for AVX spills, and this code runs inside other people's games.

// 32-bit pixels with the alpha byte forced to 255.
void CopyOpaqueRow(const uint8_t* src, uint8_t* dst, uint32_t width) {
    const __m128i alpha = _mm_set1_epi32(int(0xFF000000));
    uint32_t x = 0;
    for (; x + 4 <= width; x += 4, src += 16, dst += 16) {
        __m128i p = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), _mm_or_si128(p, alpha));
    }
    for (; x < width; ++x, src += 4, dst += 4) {
        dst[0] = src[0], dst[1] = src[1], dst[2] = src[2], dst[3] = 255;
    }
}

// 10-bit red and blue swapped, green and alpha kept.
void SwapRedBlue10Row(const uint8_t* src, uint8_t* dst, uint32_t width) {
    const __m128i keep = _mm_set1_epi32(int(0xC00FFC00));
    const __m128i low10 = _mm_set1_epi32(0x3FF);
    uint32_t x = 0;
    for (; x + 4 <= width; x += 4, src += 16, dst += 16) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
        __m128i out = _mm_or_si128(_mm_and_si128(v, keep),
                                   _mm_or_si128(_mm_slli_epi32(_mm_and_si128(v, low10), 20),
                                                _mm_and_si128(_mm_srli_epi32(v, 20), low10)));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), out);
    }
    for (; x < width; ++x, src += 4, dst += 4) {
        uint32_t v;
        memcpy(&v, src, 4);
        v = (v & 0xC00FFC00) | ((v & 0x3FF) << 20) | ((v >> 20) & 0x3FF);
        memcpy(dst, &v, 4);
    }
}

// 5- or 6-bit channels in 16-bit lanes, widened to 8 bits as Expand5 and Expand6 do.
inline __m128i Expand5x8(__m128i v) {
    return _mm_or_si128(_mm_slli_epi16(v, 3), _mm_srli_epi16(v, 2));
}
inline __m128i Expand6x8(__m128i v) {
    return _mm_or_si128(_mm_slli_epi16(v, 2), _mm_srli_epi16(v, 4));
}

// 16-bit pixels to BGRA8: blue in bits 0-4, green from bit 5 (`greenBits` wide), red above.
// Computing the channels is cheaper than looking each pixel up in a table: 8 pixels take a
// dozen instructions.
template <int greenBits>
void Expand16Row(const uint8_t* src, uint8_t* dst, uint32_t width) {
    const __m128i five = _mm_set1_epi16(0x1F);
    const __m128i green = _mm_set1_epi16((1 << greenBits) - 1);
    const __m128i alpha = _mm_set1_epi16(int16_t(0xFF00));
    uint32_t x = 0;
    for (; x + 8 <= width; x += 8, src += 16, dst += 32) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
        __m128i b = Expand5x8(_mm_and_si128(v, five));
        __m128i g = _mm_and_si128(_mm_srli_epi16(v, 5), green);
        g = greenBits == 6 ? Expand6x8(g) : Expand5x8(g);
        __m128i r = Expand5x8(_mm_and_si128(_mm_srli_epi16(v, 5 + greenBits), five));
        // Each lane holds 8 bits, so B | G << 8 and R | A << 8 are the pixel's two halves.
        __m128i bg = _mm_or_si128(b, _mm_slli_epi16(g, 8));
        __m128i ra = _mm_or_si128(r, alpha);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), _mm_unpacklo_epi16(bg, ra));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), _mm_unpackhi_epi16(bg, ra));
    }
    for (; x < width; ++x, src += 2, dst += 4) {
        uint32_t v = uint32_t(src[0]) | (uint32_t(src[1]) << 8);
        uint32_t g = (v >> 5) & ((1u << greenBits) - 1);
        dst[0] = Expand5(v & 0x1F);
        dst[1] = greenBits == 6 ? Expand6(g) : Expand5(g);
        dst[2] = Expand5((v >> (5 + greenBits)) & 0x1F);
        dst[3] = 255;
    }
}

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
            CopyOpaqueRow(src, dst, width);
            break;
        case PixelLayout::BGR10A2:
            SwapRedBlue10Row(src, dst, width);
            break;
        case PixelLayout::B5G6R5:
            Expand16Row<6>(src, dst, width);
            break;
        case PixelLayout::B5G5R5X1:
            Expand16Row<5>(src, dst, width);
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
