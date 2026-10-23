#include "gazeora/input/gestures/finger_straightness.h"

namespace gazeora::input {

float FingerStraightness::Of(const HandLandmarks& hand, Finger finger) {
    int knuckle = int(KnuckleOf(finger));
    float along = 0.0f;
    for (int joint = knuckle; joint < int(TipOf(finger)); ++joint) {
        along += hand.All()[joint].DistanceTo(hand.All()[joint + 1]);
    }
    float across = hand.All()[knuckle].DistanceTo(hand.At(TipOf(finger)));
    return along > 0.0f ? across / along : 0.0f;
}

}  // namespace gazeora::input
