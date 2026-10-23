#include "gazeora/input/gestures/landmark_smoother.h"

#include <gtest/gtest.h>

#include <cmath>
#include <random>

namespace gazeora::input {
namespace {

constexpr double kFrame = 1.0 / 30;

HandLandmarks HandAt(float x, float y) {
    std::array<Landmark, kHandJointCount> joints;
    joints.fill({x, y, 0.0f});
    return HandLandmarks(joints, 1.0f);
}

TEST(LandmarkSmootherTest, SteadiesAStillHand) {
    LandmarkSmoother smoother;
    std::mt19937 random(3);
    std::normal_distribution<float> shake(0.0f, 1.5f);  // pixels, as the model's joints
    float squares = 0.0f;
    int counted = 0;
    for (int frame = 0; frame < 300; ++frame) {
        HandLandmarks smooth =
            smoother.Smooth(HandAt(300.0f + shake(random), 200.0f + shake(random)), frame * kFrame);
        if (frame >= 30) {
            float off = smooth.At(HandJoint::IndexTip).x - 300.0f;
            squares += off * off;
            ++counted;
        }
    }
    EXPECT_LT(std::sqrt(squares / counted), 0.5f);  // a third of the shake, or less
}

TEST(LandmarkSmootherTest, KeepsUpWithAMovingHand) {
    LandmarkSmoother smoother;
    HandLandmarks smooth = HandAt(0.0f, 0.0f);
    for (int frame = 0; frame <= 15; ++frame) {
        // 300 pixels a second.
        smooth = smoother.Smooth(HandAt(frame * 10.0f, 0.0f), frame * kFrame);
    }
    EXPECT_GT(smooth.At(HandJoint::Wrist).x, 130.0f);  // of 150
}

}  // namespace
}  // namespace gazeora::input
