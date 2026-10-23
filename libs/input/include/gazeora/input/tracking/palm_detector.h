#pragma once

#include <vector>

#include "gazeora/input/interfaces/inference_model.h"
#include "gazeora/input/models/camera_frame.h"
#include "gazeora/input/models/hand_roi.h"
#include "gazeora/input/models/palm.h"
#include "gazeora/input/tracking/image_sampler.h"
#include "gazeora/input/tracking/palm_anchors.h"

namespace gazeora::input {

// Finds palms in a whole camera picture with MediaPipe's palm detection model.
class PalmDetector {
public:
    static constexpr int kInputSide = 192;
    static constexpr float kDefaultMinScore = 0.5f;

    // Throws InputError if `model` doesn't take a 192x192 RGB picture.
    PalmDetector(IInferenceModel& model, const ImageSampler& sampler,
                 float minScore = kDefaultMinScore);

    // Up to `most` different palms scoring at least the minimum, the likeliest first. Throws
    // InputError if the model's output isn't the palm detector's.
    std::vector<Palm> Detect(const CameraFrame& frame, size_t most);

private:
    // The whole picture, centered in a square (letterboxed).
    static HandRoi WholeFrame(const CameraFrame& frame);
    // The anchors scoring at least the minimum, the likeliest first.
    std::vector<size_t> LikelyAnchors(const std::vector<float>& scores) const;
    Palm DecodePalm(const std::vector<float>& regressors, size_t anchor, float score,
                    const HandRoi& input) const;

    IInferenceModel& m_model;
    const ImageSampler& m_sampler;
    PalmAnchors m_anchors;
    float m_minScore;
};

}  // namespace gazeora::input
