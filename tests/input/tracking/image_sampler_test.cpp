#include "gazeora/input/tracking/image_sampler.h"

#include <gtest/gtest.h>

namespace gazeora::input {
namespace {

constexpr int kSide = 4;

// A 4x4 picture whose red channel is 10 * x + y and blue channel 100.
CameraFrame Gradient() {
    std::vector<uint8_t> pixels;
    for (int y = 0; y < kSide; ++y) {
        for (int x = 0; x < kSide; ++x) {
            pixels.insert(pixels.end(), {100, 0, uint8_t(10 * x + y), 0});
        }
    }
    return CameraFrame(kSide, kSide, pixels);
}

float RedAt(const std::vector<float>& tensor, int column, int row, int side) {
    return tensor[(size_t(row) * side + column) * ImageSampler::kChannels] * 255.0f;
}

TEST(ImageSamplerTest, CopiesAnUprightRegionPixelForPixel) {
    ImageSampler sampler;
    HandRoi whole({2.0f, 2.0f}, 4.0f, {0.0f, -1.0f});
    std::vector<float> tensor = sampler.Sample(Gradient(), whole, kSide);
    ASSERT_EQ(tensor.size(), size_t(kSide * kSide * 3));
    EXPECT_NEAR(RedAt(tensor, 3, 1, kSide), 31.0f, 1e-3f);
    EXPECT_NEAR(tensor[2] * 255.0f, 100.0f, 1e-3f);  // RGB order: blue last
}

TEST(ImageSamplerTest, TurnsTheRegionWithItsUpDirection) {
    ImageSampler sampler;
    // "Up" in the crop points right in the picture: the crop's top row is the picture's
    // right column.
    HandRoi turned({2.0f, 2.0f}, 4.0f, {1.0f, 0.0f});
    std::vector<float> tensor = sampler.Sample(Gradient(), turned, kSide);
    EXPECT_NEAR(RedAt(tensor, 0, 0, kSide), 30.0f, 1e-3f);  // picture (3, 0)
    EXPECT_NEAR(RedAt(tensor, 3, 0, kSide), 33.0f, 1e-3f);  // picture (3, 3)
}

TEST(ImageSamplerTest, OutsideThePictureIsBlack) {
    ImageSampler sampler;
    HandRoi doubled({2.0f, 2.0f}, 8.0f, {0.0f, -1.0f});
    std::vector<float> tensor = sampler.Sample(Gradient(), doubled, kSide);
    EXPECT_FLOAT_EQ(RedAt(tensor, 0, 0, kSide), 0.0f);
}

}  // namespace
}  // namespace gazeora::input
