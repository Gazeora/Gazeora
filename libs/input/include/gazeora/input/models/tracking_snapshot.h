#pragma once

#include <array>
#include <optional>
#include <string>

#include "gazeora/input/gestures/hand_pose.h"
#include "gazeora/input/models/hand_joint.h"
#include "gazeora/input/models/vec2.h"

namespace gazeora::input {

enum class TrackingState { Off, Starting, NoHand, Tracking, Error };

// What hand tracking is doing right now, for the UI.
struct TrackingSnapshot {
    TrackingState state = TrackingState::Off;
    // Joints from 0 to 1 across the camera picture, mirrored like a selfie view.
    std::array<Vec2, kHandJointCount> joints{};
    // The other hand's joints, the same way, when it's in view: its pinch scrolls.
    std::optional<std::array<Vec2, kHandJointCount>> otherHand;
    float cameraAspect = 1.0f;  // width / height
    GestureMode mode = GestureMode::Resting;
    bool pointerHeld = false;  // the fingers are folding: the pointer waits for the click
    bool handOpen = false;     // every finger up: held, it brings the pointer back
    int clicks = 0;            // since tracking started
    float framesPerSecond = 0.0f;
    std::wstring message;  // why, in the Error state
};

}  // namespace gazeora::input
