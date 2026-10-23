#pragma once

#include "gazeora/input/interfaces/inference_model.h"
#include "gazeora/input/models/camera_frame.h"
#include "gazeora/input/models/hand_landmarks.h"
#include "gazeora/input/models/hand_roi.h"
#include "gazeora/input/tracking/image_sampler.h"

namespace gazeora::input {

// Finds the 21 joints of the hand in a region with MediaPipe's hand landmark model.
class LandmarkEstimator {
public:
    static constexpr int kInputSide = 224;

    // Throws InputError if `model` doesn't take a 224x224 RGB picture.
    LandmarkEstimator(IInferenceModel& model, const ImageSampler& sampler);

    // The joints in camera pixels. Throws InputError if the model's output isn't the hand
    // landmark model's.
    HandLandmarks Estimate(const CameraFrame& frame, const HandRoi& roi);

private:
    static Landmark ToImage(const float* xyz, const HandRoi& roi);
    // The model's 3D joints, if it gives them (MediaPipe's world landmarks).
    static HandLandmarks::WorldJoints WorldOf(const float* xyz);

    IInferenceModel& m_model;
    const ImageSampler& m_sampler;
};

}  // namespace gazeora::input
