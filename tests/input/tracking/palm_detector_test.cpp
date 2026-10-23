#include "gazeora/input/tracking/palm_detector.h"

#include <gtest/gtest.h>

#include <cmath>

#include "gazeora/input/tracking/palm_anchors.h"

namespace gazeora::input {
namespace {

constexpr size_t kValuesPerAnchor = 18;  // box (4), then 7 keypoints (x, y)
constexpr float kUnlikely = -10.0f;      // a score's logit: far below the minimum
// Anchors: two in one cell of the 24x24 grid (the same palm), one far away, one too unsure.
constexpr size_t kPalm = (5 * 24 + 5) * 2, kSamePalm = kPalm + 1;
constexpr size_t kOtherPalm = (18 * 24 + 18) * 2, kUnsure = (10 * 24 + 18) * 2;

// The palm model's outputs for a 192x192 picture, with palms at the given anchors: each one's
// wrist 10 pixels below its anchor and middle finger knuckle 10 above.
class ScriptedPalmModel final : public IInferenceModel {
public:
    size_t InputSize() const override {
        return size_t(PalmDetector::kInputSide) * PalmDetector::kInputSide * 3;
    }
    std::vector<std::vector<float>> Run(const std::vector<float>&) override {
        size_t anchors = PalmAnchors().All().size();
        std::vector<float> regressors(anchors * kValuesPerAnchor, 0.0f);
        std::vector<float> scores(anchors, kUnlikely);
        for (auto [anchor, logit] :
             {std::pair{kPalm, 3.0f}, {kSamePalm, 2.5f}, {kOtherPalm, 2.0f}, {kUnsure, -1.0f}}) {
            scores[anchor] = logit;
            regressors[anchor * kValuesPerAnchor + 5] = 10.0f;   // wrist y
            regressors[anchor * kValuesPerAnchor + 9] = -10.0f;  // middle knuckle y
        }
        return {regressors, scores};
    }
};

class PalmDetectorTest : public ::testing::Test {
protected:
    ScriptedPalmModel m_model;
    ImageSampler m_sampler;
    PalmDetector m_detector{m_model, m_sampler};
    CameraFrame m_frame{PalmDetector::kInputSide, PalmDetector::kInputSide,
                        std::vector<uint8_t>(size_t(192) * 192 * 4)};
};

TEST_F(PalmDetectorTest, FindsEachLikelyPalmOnceLikeliestFirst) {
    std::vector<Palm> palms = m_detector.Detect(m_frame, 3);
    ASSERT_EQ(palms.size(), 2u);
    EXPECT_FLOAT_EQ(palms[0].Score(), 1.0f / (1.0f + std::exp(-3.0f)));
    EXPECT_FLOAT_EQ(palms[1].Score(), 1.0f / (1.0f + std::exp(-2.0f)));
    EXPECT_GT(palms[1].Keypoints()[Palm::kWristKeypoint].x, 100.0f);  // the far one
}

TEST_F(PalmDetectorTest, FindsNoMoreThanAsked) {
    EXPECT_EQ(m_detector.Detect(m_frame, 1).size(), 1u);
}

}  // namespace
}  // namespace gazeora::input
