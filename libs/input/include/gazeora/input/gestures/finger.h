#pragma once

#include "gazeora/input/models/hand_joint.h"

namespace gazeora::input {

enum class Finger { Index, Middle, Ring, Pinky };

// Each finger's joints, knuckle to tip, are consecutive in MediaPipe's order.
inline constexpr int kJointsPerFinger = 4;

inline HandJoint KnuckleOf(Finger finger) {
    switch (finger) {
        case Finger::Index:
            return HandJoint::IndexMcp;
        case Finger::Middle:
            return HandJoint::MiddleMcp;
        case Finger::Ring:
            return HandJoint::RingMcp;
        case Finger::Pinky:
            return HandJoint::PinkyMcp;
    }
    return HandJoint::IndexMcp;
}

inline HandJoint TipOf(Finger finger) {
    return HandJoint(int(KnuckleOf(finger)) + kJointsPerFinger - 1);
}

}  // namespace gazeora::input
