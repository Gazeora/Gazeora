#pragma once

#include "gazeora/input/models/hand_landmarks.h"
#include "gazeora/input/models/vec2.h"

namespace gazeora::input {

// The index fingertip as the camera sees it, and the hand's size to measure its moves by.
class Fingertip {
public:
    // In camera pixels, mirrored like a selfie view: x grows to the user's right.
    static Vec2 Position(const HandLandmarks& hand);
    // The palm's length (wrist to middle knuckle) in camera pixels. The palm keeps its shape
    // however the fingers move; its length shrinks when the hand tips forward and its width
    // when it turns sideways, so the larger of the two stands for it.
    static float PalmLength(const HandLandmarks& hand);
};

}  // namespace gazeora::input
