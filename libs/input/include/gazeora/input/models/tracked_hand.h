#pragma once

#include "gazeora/input/models/hand_landmarks.h"

namespace gazeora::input {

// One hand in a camera picture, and which hand it is from picture to picture: the tracker
// gives it an id when it first finds it and keeps it while it follows it.
struct TrackedHand {
    int id;
    HandLandmarks landmarks;
};

}  // namespace gazeora::input
