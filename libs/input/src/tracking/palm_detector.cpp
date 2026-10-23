#include "gazeora/input/tracking/palm_detector.h"

#include <algorithm>
#include <cmath>

#include "gazeora/input/exceptions/input_error.h"

namespace gazeora::input {

namespace {

// Each anchor's regression: box center and size (4 values), then 7 keypoints (x, y).
constexpr size_t kValuesPerAnchor = 4 + Palm::kKeypointCount * 2;
constexpr size_t kKeypointOffset = 4;
constexpr size_t kRegressorOutput = 0, kScoreOutput = 1;
constexpr Vec2 kUp{0.0f, -1.0f};
constexpr float kHalf = 0.5f;

float Sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

}  // namespace

PalmDetector::PalmDetector(IInferenceModel& model, const ImageSampler& sampler, float minScore)
    : m_model(model), m_sampler(sampler), m_minScore(minScore) {
    if (model.InputSize() != size_t(kInputSide) * kInputSide * ImageSampler::kChannels) {
        throw InputError(L"The palm detection model takes an unexpected input.");
    }
}

std::vector<Palm> PalmDetector::Detect(const CameraFrame& frame, size_t most) {
    HandRoi input = WholeFrame(frame);
    std::vector<std::vector<float>> outputs =
        m_model.Run(m_sampler.Sample(frame, input, kInputSide));
    size_t anchors = m_anchors.All().size();
    if (outputs.size() <= kScoreOutput || outputs[kScoreOutput].size() != anchors ||
        outputs[kRegressorOutput].size() != anchors * kValuesPerAnchor) {
        throw InputError(L"The palm detection model gave an unexpected output.");
    }
    std::vector<Palm> palms;
    for (size_t anchor : LikelyAnchors(outputs[kScoreOutput])) {
        float score = Sigmoid(outputs[kScoreOutput][anchor]);
        Palm palm = DecodePalm(outputs[kRegressorOutput], anchor, score, input);
        bool seen = std::any_of(palms.begin(), palms.end(),
                                [&](const Palm& found) { return found.Overlaps(palm); });
        if (!seen && palms.size() < most) {
            palms.push_back(palm);
        }
    }
    return palms;
}

HandRoi PalmDetector::WholeFrame(const CameraFrame& frame) {
    Vec2 center{frame.Width() * kHalf, frame.Height() * kHalf};
    return HandRoi(center, float(std::max(frame.Width(), frame.Height())), kUp);
}

std::vector<size_t> PalmDetector::LikelyAnchors(const std::vector<float>& scores) const {
    std::vector<size_t> anchors;
    for (size_t i = 0; i < scores.size(); ++i) {
        if (Sigmoid(scores[i]) >= m_minScore) {
            anchors.push_back(i);
        }
    }
    std::sort(anchors.begin(), anchors.end(),
              [&](size_t a, size_t b) { return scores[a] > scores[b]; });
    return anchors;
}

Palm PalmDetector::DecodePalm(const std::vector<float>& regressors, size_t anchor, float score,
                              const HandRoi& input) const {
    const float* values = regressors.data() + anchor * kValuesPerAnchor + kKeypointOffset;
    Vec2 origin = m_anchors.All()[anchor] - Vec2{kHalf, kHalf};
    std::array<Vec2, Palm::kKeypointCount> keypoints;
    for (int i = 0; i < Palm::kKeypointCount; ++i) {
        Vec2 offset{values[i * 2] / kInputSide, values[i * 2 + 1] / kInputSide};
        keypoints[i] = input.ToImage(origin + offset);
    }
    return Palm(score, keypoints);
}

}  // namespace gazeora::input
