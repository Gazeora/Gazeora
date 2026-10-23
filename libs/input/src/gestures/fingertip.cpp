#include "gazeora/input/gestures/fingertip.h"

#include <algorithm>

namespace gazeora::input {

namespace {

// The knuckles' span is about 0.72 of the palm's length, measured on MediaPipe's sample
// photos.
constexpr float kSpanPerPalm = 0.72f;

}  // namespace

Vec2 Fingertip::Position(const HandLandmarks& hand) {
    Vec2 tip = hand.At(HandJoint::IndexTip).Xy();
    return {-tip.x, tip.y};  // the camera faces the user: their right is its left
}

float Fingertip::PalmLength(const HandLandmarks& hand) {
    float palm = (hand.At(HandJoint::MiddleMcp).Xy() - hand.At(HandJoint::Wrist).Xy()).Length();
    float span = (hand.At(HandJoint::PinkyMcp).Xy() - hand.At(HandJoint::IndexMcp).Xy()).Length();
    return std::max(palm, span / kSpanPerPalm);
}

}  // namespace gazeora::input
