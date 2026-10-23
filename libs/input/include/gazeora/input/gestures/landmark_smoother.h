#pragma once

#include <array>

#include "gazeora/input/gestures/one_euro_filter.h"
#include "gazeora/input/models/hand_landmarks.h"

namespace gazeora::input {

// Steadies the 21 joints from picture to picture, each with its own 1-euro filter: the
// landmark model's joints shake by a pixel or two on a still hand.
class LandmarkSmoother {
public:
    LandmarkSmoother();

    HandLandmarks Smooth(const HandLandmarks& hand, double now);
    // The hand was lost: the next one starts fresh.
    void Reset();

private:
    std::array<OneEuroFilter<Vec3>, kHandJointCount> m_filters;
};

}  // namespace gazeora::input
