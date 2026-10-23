#pragma once

#include <array>
#include <cmath>

#include "gazeora/input/models/hand_landmarks.h"
#include "gazeora/input/models/vec3.h"

namespace gazeora::input::testing {

// Bends of a finger's middle joint, in degrees; its end joint bends two thirds as much.
// FingerStraightness of each: 1, about 0.86 (between the lines), 0.75 (half folded), 0.33.
constexpr float kStraight = 0.0f;
constexpr float kSlightlyBent = 45.0f;
constexpr float kHalfFolded = 60.0f;
constexpr float kFolded = 100.0f;

// Hand builders, with real proportions: the wrist 100 pixels below the knuckles, the index
// finger 85 pixels long.
class Hands {
public:
    // The index finger straight along `direction` (unit length, camera space), the middle
    // finger bent by `middleBend` (folded), the whole hand moved by `shift` pixels.
    static HandLandmarks Pointing(Vec3 direction, Vec3 shift = {}, float middleBend = kFolded) {
        std::array<Landmark, kHandJointCount> joints = Palm(shift);
        SetFinger(joints, HandJoint::IndexMcp, direction, kStraight);
        SetFinger(joints, HandJoint::MiddleMcp, kUp, middleBend);
        return HandLandmarks(joints, 1.0f);
    }

    // The index and middle fingers up, their middle joints bent by these degrees.
    static HandLandmarks Folding(float indexBend, float middleBend, Vec3 shift = {}) {
        std::array<Landmark, kHandJointCount> joints = Palm(shift);
        SetFinger(joints, HandJoint::IndexMcp, kUp, indexBend);
        SetFinger(joints, HandJoint::MiddleMcp, kUp, middleBend);
        return HandLandmarks(joints, 1.0f);
    }

    // `hand` with its thumb tip `gap` pixels beside the index fingertip: pinching, when small
    // (the palm is 102 pixels long).
    static HandLandmarks ThumbTo(const HandLandmarks& hand, float gap = 5.0f) {
        std::array<Landmark, kHandJointCount> joints = hand.All();
        Landmark tip = joints[int(HandJoint::IndexTip)];
        joints[int(HandJoint::ThumbTip)] = {tip.x + gap, tip.y, tip.z};
        return HandLandmarks(joints, 1.0f);
    }

    // `hand` taken for a left one.
    static HandLandmarks AsLeft(const HandLandmarks& hand) {
        return HandLandmarks(hand.All(), hand.AllWorld(), hand.Confidence(),
                             HandLandmarks::kLeftHand);
    }

    // Every finger straight up: an open hand.
    static HandLandmarks Open() {
        std::array<Landmark, kHandJointCount> joints = Palm({});
        for (HandJoint knuckle :
             {HandJoint::IndexMcp, HandJoint::MiddleMcp, HandJoint::RingMcp, HandJoint::PinkyMcp}) {
            SetFinger(joints, knuckle, kUp, kStraight);
        }
        return HandLandmarks(joints, 1.0f);
    }

private:
    static constexpr float kKnuckleHeight = 100.0f;
    static constexpr float kKnuckleGap = 20.0f;
    static constexpr float kFingerLength = 85.0f;
    // Knuckle to middle joint, middle to end joint, end joint to tip.
    static constexpr float kSegments[] = {0.45f, 0.28f, 0.24f};
    static constexpr float kEndBendShare = 2.0f / 3.0f;
    static constexpr float kRadiansPerDegree = 3.14159265f / 180.0f;
    static constexpr Vec3 kUp{0.0f, -1.0f, 0.0f};

    static std::array<Landmark, kHandJointCount> Palm(Vec3 shift) {
        Vec3 wrist = Vec3{500.0f, 600.0f, 0.0f} + shift;
        std::array<Landmark, kHandJointCount> joints{};
        for (int i = 0; i < kHandJointCount; ++i) {
            Place(joints, HandJoint(i), wrist);
        }
        HandJoint knuckles[] = {HandJoint::IndexMcp, HandJoint::MiddleMcp, HandJoint::RingMcp,
                                HandJoint::PinkyMcp};
        for (int i = 0; i < 4; ++i) {
            Place(joints, knuckles[i], wrist + Vec3{kKnuckleGap * i, -kKnuckleHeight, 0.0f});
        }
        SetFinger(joints, HandJoint::RingMcp, kUp, kFolded);
        SetFinger(joints, HandJoint::PinkyMcp, kUp, kFolded);
        for (int i = 0; i < 4; ++i) {
            Place(joints, HandJoint(int(HandJoint::ThumbCmc) + i), wrist + kTucked[i]);
        }
        return joints;
    }
    // The thumb tucked across the folded fingers, its tip by the middle of the palm: well away
    // from the index fingertip, however the index finger points.
    static constexpr Vec3 kTucked[] = {{-15.0f, -20.0f, 0.0f},
                                       {-30.0f, -45.0f, 0.0f},
                                       {-30.0f, -65.0f, 0.0f},
                                       {-20.0f, -80.0f, 0.0f}};
    // Lays a finger out from its knuckle along `direction`, curling toward the palm (turning
    // within the picture) by `bend` at the middle joint and two thirds of it at the end.
    static void SetFinger(std::array<Landmark, kHandJointCount>& joints, HandJoint knuckle,
                          Vec3 direction, float bend) {
        float turns[] = {0.0f, bend, bend * (1.0f + kEndBendShare)};
        Vec3 at = joints[int(knuckle)].Xyz();
        for (int segment = 0; segment < 3; ++segment) {
            at = at + Turned(direction, turns[segment]) * (kSegments[segment] * kFingerLength);
            Place(joints, HandJoint(int(knuckle) + segment + 1), at);
        }
    }
    static Vec3 Turned(Vec3 direction, float degrees) {
        float c = std::cos(degrees * kRadiansPerDegree), s = std::sin(degrees * kRadiansPerDegree);
        return {direction.x * c - direction.y * s, direction.x * s + direction.y * c, direction.z};
    }
    static void Place(std::array<Landmark, kHandJointCount>& joints, HandJoint joint, Vec3 at) {
        joints[int(joint)] = {at.x, at.y, at.z};
    }
};

}  // namespace gazeora::input::testing
