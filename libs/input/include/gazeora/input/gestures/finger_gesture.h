#pragma once

#include "gazeora/input/gestures/hand_pose.h"

namespace gazeora::input {

// What one picture of the hand means for the pointer.
struct FingerGesture {
    GestureMode mode = GestureMode::Resting;
    bool clicked = false;  // the right index just opened again after a fold

    bool PointerHeld() const { return mode != GestureMode::Steering; }
};

}  // namespace gazeora::input
