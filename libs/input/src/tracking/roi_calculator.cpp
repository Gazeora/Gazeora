#include "gazeora/input/tracking/roi_calculator.h"

#include <algorithm>
#include <cfloat>

namespace gazeora::input {

namespace {

// The palm keypoints cover only the palm: reach out to the fingertips.
constexpr float kPalmScale = 3.0f;
constexpr float kPalmShift = 0.4f;
// The palm and the bases of the fingers: the joints that stay put while fingers fold and
// shake least, so the region doesn't wander with the fingertips (MediaPipe's choice).
constexpr HandJoint kSteadyJoints[] = {
    HandJoint::Wrist,    HandJoint::ThumbCmc, HandJoint::ThumbMcp,  HandJoint::ThumbIp,
    HandJoint::IndexMcp, HandJoint::IndexPip, HandJoint::MiddleMcp, HandJoint::MiddlePip,
    HandJoint::RingMcp,  HandJoint::RingPip,  HandJoint::PinkyMcp,  HandJoint::PinkyPip,
};
// Twice the palm and finger bases: room for the fingers and for the hand to move until the
// next picture.
constexpr float kHandScale = 2.0f;
constexpr float kHandShift = 0.1f;
constexpr float kHalf = 0.5f;
constexpr Vec2 kUp{0.0f, -1.0f};

Vec2 Direction(Vec2 from, Vec2 to) {
    Vec2 delta = to - from;
    float length = delta.Length();
    return length > 0.0f ? delta * (1.0f / length) : kUp;
}

}  // namespace

HandRoi RoiCalculator::FromPalm(const Palm& palm) const {
    const auto& keypoints = palm.Keypoints();
    return Around({keypoints.begin(), keypoints.end()}, keypoints[Palm::kWristKeypoint],
                  keypoints[Palm::kMiddleFingerKeypoint], kPalmScale, kPalmShift);
}

HandRoi RoiCalculator::FromLandmarks(const HandLandmarks& hand) const {
    std::vector<Vec2> points;
    for (HandJoint joint : kSteadyJoints) {
        points.push_back(hand.At(joint).Xy());
    }
    // Toward the knuckles, weighted to the middle finger, as MediaPipe does.
    Vec2 sides = (hand.At(HandJoint::IndexMcp).Xy() + hand.At(HandJoint::RingMcp).Xy()) * kHalf;
    Vec2 knuckles = (sides + hand.At(HandJoint::MiddleMcp).Xy()) * kHalf;
    return Around(points, hand.At(HandJoint::Wrist).Xy(), knuckles, kHandScale, kHandShift);
}

HandRoi RoiCalculator::Around(const std::vector<Vec2>& points, Vec2 base, Vec2 tip, float scale,
                              float shift) {
    Vec2 up = Direction(base, tip), right = up.Perpendicular();
    Vec2 origin = points.front();
    float minRight = FLT_MAX, maxRight = -FLT_MAX, minUp = FLT_MAX, maxUp = -FLT_MAX;
    for (Vec2 point : points) {
        float alongRight = (point - origin).Dot(right), alongUp = (point - origin).Dot(up);
        minRight = std::min(minRight, alongRight);
        maxRight = std::max(maxRight, alongRight);
        minUp = std::min(minUp, alongUp);
        maxUp = std::max(maxUp, alongUp);
    }
    float width = maxRight - minRight, height = maxUp - minUp;
    float centerRight = (minRight + maxRight) * kHalf;
    float centerUp = (minUp + maxUp) * kHalf + shift * height;
    Vec2 center = origin + right * centerRight + up * centerUp;
    return HandRoi(center, std::max(width, height) * scale, up);
}

}  // namespace gazeora::input
