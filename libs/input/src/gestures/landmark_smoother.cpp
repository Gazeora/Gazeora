#include "gazeora/input/gestures/landmark_smoother.h"

namespace gazeora::input {

namespace {

// Tuned in camera pixels: a still hand shakes at a few pixels a second, a moving one goes
// hundreds, which lifts the cutoff so it doesn't lag.
constexpr OneEuroTuning kJointTuning{0.5f, 0.01f, 1.0f};

}  // namespace

LandmarkSmoother::LandmarkSmoother() { m_filters.fill(OneEuroFilter<Vec3>(kJointTuning)); }

HandLandmarks LandmarkSmoother::Smooth(const HandLandmarks& hand, double now) {
    HandLandmarks::Joints joints;
    for (int i = 0; i < kHandJointCount; ++i) {
        Vec3 smooth = m_filters[i].Filter(hand.All()[i].Xyz(), now);
        joints[i] = {smooth.x, smooth.y, smooth.z};
    }
    return HandLandmarks(joints, hand.AllWorld(), hand.Confidence(), hand.RightHand());
}

void LandmarkSmoother::Reset() {
    for (OneEuroFilter<Vec3>& filter : m_filters) {
        filter.Reset();
    }
}

}  // namespace gazeora::input
