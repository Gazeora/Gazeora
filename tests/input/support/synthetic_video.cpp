#include "support/synthetic_video.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace gazeora::input::testing {

namespace {

constexpr float kRadiansPerDegree = 3.14159265f / 180.0f;
constexpr float kHalfFieldOfViewDegrees = 30.0f;
constexpr uint8_t kBackground = 128;

}  // namespace

CameraFrame SyntheticVideo::Frame(Vec2 center, float rollDegrees, float pitchDegrees) const {
    std::vector<uint8_t> pixels(size_t(kWidth) * kHeight * CameraFrame::kBytesPerPixel,
                                kBackground);
    uint8_t* out = pixels.data();
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x, out += CameraFrame::kBytesPerPixel) {
            Vec2 from = PhotoPoint(Vec2{x + 0.5f, y + 0.5f} - center, rollDegrees, pitchDegrees);
            int px = int(std::floor(from.x)), py = int(std::floor(from.y));
            if (px >= 0 && py >= 0 && px < m_photo.Width() && py < m_photo.Height()) {
                std::copy_n(m_photo.Pixel(px, py), CameraFrame::kBytesPerPixel, out);
            }
        }
    }
    return CameraFrame(kWidth, kHeight, std::move(pixels));
}

CameraFrame SyntheticVideo::SideBySide(const CameraFrame& right, const CameraFrame& left) {
    int width = right.Width() + left.Width(), height = std::max(right.Height(), left.Height());
    std::vector<uint8_t> pixels(size_t(width) * height * CameraFrame::kBytesPerPixel, kBackground);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool first = x < right.Width();
            const CameraFrame& from = first ? right : left;
            int fromX = first ? x : left.Width() - 1 - (x - right.Width());
            if (y < from.Height()) {
                std::copy_n(from.Pixel(fromX, y), CameraFrame::kBytesPerPixel,
                            &pixels[(size_t(y) * width + x) * CameraFrame::kBytesPerPixel]);
            }
        }
    }
    return CameraFrame(width, height, std::move(pixels));
}

Vec2 SyntheticVideo::PhotoPoint(Vec2 offset, float rollDegrees, float pitchDegrees) const {
    // Undo the tip: a card's row at height h (negative above the middle), its top turned
    // toward a camera at distance f, sits at depth f + h sin and is seen at
    // v = f h cos / (f + h sin).
    float focal = kWidth / (2.0f * std::tan(kHalfFieldOfViewDegrees * kRadiansPerDegree));
    float pitch = pitchDegrees * kRadiansPerDegree;
    float height = offset.y * focal / (focal * std::cos(pitch) - offset.y * std::sin(pitch));
    float across = offset.x * (focal + height * std::sin(pitch)) / focal;
    // Then the turn in the picture.
    float roll = -rollDegrees * kRadiansPerDegree;
    Vec2 unturned{across * std::cos(roll) - height * std::sin(roll),
                  across * std::sin(roll) + height * std::cos(roll)};
    return unturned + Vec2{m_photo.Width() * 0.5f, m_photo.Height() * 0.5f};
}

}  // namespace gazeora::input::testing
