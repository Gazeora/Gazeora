#include "gazeora/input/tracking/image_sampler.h"

#include <cmath>

namespace gazeora::input {

namespace {

constexpr float kMaxByte = 255.0f;
constexpr float kPixelCenter = 0.5f;
// CameraFrame's byte order is B, G, R.
constexpr int kRed = 2, kGreen = 1, kBlue = 0;

}  // namespace

std::vector<float> ImageSampler::Sample(const CameraFrame& frame, const HandRoi& roi,
                                        int side) const {
    std::vector<float> tensor(size_t(side) * side * kChannels);
    float* out = tensor.data();
    for (int row = 0; row < side; ++row) {
        for (int column = 0; column < side; ++column, out += kChannels) {
            Vec2 crop{(column + kPixelCenter) / side - kPixelCenter,
                      (row + kPixelCenter) / side - kPixelCenter};
            SampleAt(frame, roi.ToImage(crop), out);
        }
    }
    return tensor;
}

void ImageSampler::SampleAt(const CameraFrame& frame, Vec2 point, float* rgb) {
    // Pixel centers sit at half coordinates.
    float x = point.x - kPixelCenter, y = point.y - kPixelCenter;
    int left = int(std::floor(x)), top = int(std::floor(y));
    float fx = x - left, fy = y - top;
    rgb[0] = rgb[1] = rgb[2] = 0.0f;
    AddPixel(frame, left, top, (1 - fx) * (1 - fy), rgb);
    AddPixel(frame, left + 1, top, fx * (1 - fy), rgb);
    AddPixel(frame, left, top + 1, (1 - fx) * fy, rgb);
    AddPixel(frame, left + 1, top + 1, fx * fy, rgb);
}

void ImageSampler::AddPixel(const CameraFrame& frame, int x, int y, float weight, float* rgb) {
    if (x < 0 || y < 0 || x >= frame.Width() || y >= frame.Height()) {
        return;
    }
    const uint8_t* pixel = frame.Pixel(x, y);
    float scale = weight / kMaxByte;
    rgb[0] += pixel[kRed] * scale;
    rgb[1] += pixel[kGreen] * scale;
    rgb[2] += pixel[kBlue] * scale;
}

}  // namespace gazeora::input
