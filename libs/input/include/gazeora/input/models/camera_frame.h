#pragma once

#include <cstdint>
#include <vector>

#include "gazeora/input/exceptions/input_error.h"

namespace gazeora::input {

// One camera picture, top row first, 4 bytes per pixel in B, G, R, X order.
class CameraFrame {
public:
    static constexpr int kBytesPerPixel = 4;

    CameraFrame() = default;
    CameraFrame(int width, int height, std::vector<uint8_t> bgrx)
        : m_width(width), m_height(height), m_pixels(std::move(bgrx)) {
        if (width <= 0 || height <= 0 ||
            m_pixels.size() != size_t(width) * height * kBytesPerPixel) {
            throw InputError(L"A camera frame's size doesn't match its pixels.");
        }
    }

    int Width() const { return m_width; }
    int Height() const { return m_height; }
    bool Empty() const { return m_pixels.empty(); }
    const uint8_t* Pixel(int x, int y) const {
        return m_pixels.data() + (size_t(y) * m_width + x) * kBytesPerPixel;
    }

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<uint8_t> m_pixels;
};

}  // namespace gazeora::input
