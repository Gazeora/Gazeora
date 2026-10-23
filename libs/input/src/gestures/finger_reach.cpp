#include "gazeora/input/gestures/finger_reach.h"

namespace gazeora::input {

float FingerReach::Of(const HandLandmarks& hand, Finger finger) {
    float palm = (hand.World(HandJoint::MiddleMcp) - hand.World(HandJoint::Wrist)).Length();
    float reach = (hand.World(TipOf(finger)) - hand.World(KnuckleOf(finger))).Length();
    return palm > 0.0f ? reach / palm : 0.0f;
}

}  // namespace gazeora::input
