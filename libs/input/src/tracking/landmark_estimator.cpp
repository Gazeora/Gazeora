#include "gazeora/input/tracking/landmark_estimator.h"

#include "gazeora/input/exceptions/input_error.h"

namespace gazeora::input {

namespace {

constexpr size_t kValuesPerJoint = 3;  // x, y in input pixels; z relative to the wrist
constexpr size_t kLandmarkOutput = 0, kConfidenceOutput = 1, kHandednessOutput = 2;
constexpr size_t kWorldOutput = 3;
constexpr float kHalf = 0.5f;

}  // namespace

LandmarkEstimator::LandmarkEstimator(IInferenceModel& model, const ImageSampler& sampler)
    : m_model(model), m_sampler(sampler) {
    if (model.InputSize() != size_t(kInputSide) * kInputSide * ImageSampler::kChannels) {
        throw InputError(L"The hand landmark model takes an unexpected input.");
    }
}

HandLandmarks LandmarkEstimator::Estimate(const CameraFrame& frame, const HandRoi& roi) {
    std::vector<std::vector<float>> outputs = m_model.Run(m_sampler.Sample(frame, roi, kInputSide));
    if (outputs.size() <= kConfidenceOutput || outputs[kConfidenceOutput].empty() ||
        outputs[kLandmarkOutput].size() != kHandJointCount * kValuesPerJoint) {
        throw InputError(L"The hand landmark model gave an unexpected output.");
    }
    HandLandmarks::Joints joints;
    for (int i = 0; i < kHandJointCount; ++i) {
        joints[i] = ToImage(outputs[kLandmarkOutput].data() + i * kValuesPerJoint, roi);
    }
    float confidence = outputs[kConfidenceOutput][0];
    if (outputs.size() <= kWorldOutput || outputs[kHandednessOutput].empty() ||
        outputs[kWorldOutput].size() != joints.size() * kValuesPerJoint) {
        return HandLandmarks(joints, confidence);
    }
    return HandLandmarks(joints, WorldOf(outputs[kWorldOutput].data()), confidence,
                         outputs[kHandednessOutput][0]);
}

HandLandmarks::WorldJoints LandmarkEstimator::WorldOf(const float* xyz) {
    HandLandmarks::WorldJoints world;
    for (int i = 0; i < kHandJointCount; ++i, xyz += kValuesPerJoint) {
        world[i] = {xyz[0], xyz[1], xyz[2]};
    }
    return world;
}

Landmark LandmarkEstimator::ToImage(const float* xyz, const HandRoi& roi) {
    Vec2 crop{xyz[0] / kInputSide - kHalf, xyz[1] / kInputSide - kHalf};
    Vec2 image = roi.ToImage(crop);
    return {image.x, image.y, xyz[2] / kInputSide * roi.Size()};
}

}  // namespace gazeora::input
