#pragma once

namespace gazeora::input {

// What the index and middle fingers are doing.
enum class HandPose {
    Pointing,  // index up, middle folded
    TwoUp,     // both up
    Fist,      // both folded
    Other,     // index folded, middle up
};

// What the left hand says (LeftHandSigns).
enum class LeftSign {
    None,     // anything else: the pointer follows the right index finger
    IndexUp,  // hold the pointer: folding and opening the right index finger clicks
    Pinch,    // the right index finger scrolls
};

// What the hands say in one picture.
struct HandSigns {
    LeftSign left = LeftSign::None;
    bool indexUp = true;  // the right index finger
};

// What the pointer does now.
enum class GestureMode {
    Resting,    // no hand yet: still
    Steering,   // follows the right index finger
    Holding,    // still, the left index up: fold the right index and open it to click
    Folded,     // still: open the right index to click
    Scrolling,  // still, the left hand pinching: the right index finger scrolls
};

}  // namespace gazeora::input
