#pragma once

#include "gazeora/input/gestures/finger.h"
#include "gazeora/input/models/hand_landmarks.h"

namespace gazeora::input {

// How straight a finger is: the distance from its knuckle to its tip over the length along
// its joints. 1 when the joints lie in a line, whichever way the finger points and however
// the camera sees it (a line stays a line in the picture); about 0.75 bent halfway and 0.3
// folded into the palm. Bending only at the knuckle, as when pointing down, keeps it 1.
class FingerStraightness {
public:
    static float Of(const HandLandmarks& hand, Finger finger);
};

}  // namespace gazeora::input
