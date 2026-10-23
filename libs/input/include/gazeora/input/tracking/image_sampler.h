#pragma once

#include <vector>

#include "gazeora/input/models/camera_frame.h"
#include "gazeora/input/models/hand_roi.h"

namespace gazeora::input {

// Cuts a region out of a camera picture as a model's input: `side` x `side` pixels, RGB
// floats from 0 to 1, row by row (NHWC). Outside the picture is black.
class ImageSampler {
public:
    static constexpr int kChannels = 3;

    std::vector<float> Sample(const CameraFrame& frame, const HandRoi& roi, int side) const;

private:
    // Bilinear: the four pixels around an image point, weighted by how close they are.
    static void SampleAt(const CameraFrame& frame, Vec2 point, float* rgb);
    static void AddPixel(const CameraFrame& frame, int x, int y, float weight, float* rgb);
};

}  // namespace gazeora::input
