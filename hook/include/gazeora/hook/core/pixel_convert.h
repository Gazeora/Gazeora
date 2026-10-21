// Pixel layouts the CPU readback paths (D3D8, D3D9, OpenGL fallback) can hand to the
// host, and conversion of each row into a DXGI format the frame ring can hold.
#pragma once

#include <dxgiformat.h>

#include <cstdint>

namespace gazeora::hook {

// Named by byte order in memory, lowest address first.
enum class PixelLayout {
    BGRA8,        // copied as is
    BGRX8,        // alpha forced opaque
    RGBA8,        // copied as is
    RGBX8,        // alpha forced opaque
    RGB10A2,      // copied as is (red in the low bits)
    BGR10A2,      // red and blue swapped (blue in the low bits)
    B5G6R5,       // expanded to BGRA8
    B5G5R5X1,     // expanded to BGRA8
};

// The frame ring format for frames in `layout`.
DXGI_FORMAT OutputFormat(PixelLayout layout);

uint32_t SourceBytesPerPixel(PixelLayout layout);

// False if rows can be uploaded without conversion.
bool NeedsConversion(PixelLayout layout);

// Converts `width` pixels from `src` into OutputFormat(layout) at `dst`.
void ConvertRow(PixelLayout layout, const uint8_t* src, uint8_t* dst, uint32_t width);

// Maps a D3DFORMAT back buffer format (same values in D3D8 and D3D9). False if unsupported.
bool LayoutForD3DFormat(uint32_t d3dFormat, PixelLayout& layout);

}  // namespace gazeora::hook
