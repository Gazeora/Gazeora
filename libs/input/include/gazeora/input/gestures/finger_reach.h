#pragma once

#include "gazeora/input/gestures/finger.h"
#include "gazeora/input/models/hand_landmarks.h"

namespace gazeora::input {

// How far a finger reaches from its knuckle, in palm lengths, on the model's 3D joints:
// about 0.75 straight and 0.35 folded, seen from the front, the side or end-on. Where the
// picture hides a folded finger behind the others, the model may still guess its joints in a
// line, which FingerStraightness takes for straight; it rarely guesses it full length too.
class FingerReach {
public:
    // 0 if the palm has no length.
    static float Of(const HandLandmarks& hand, Finger finger);
};

}  // namespace gazeora::input
