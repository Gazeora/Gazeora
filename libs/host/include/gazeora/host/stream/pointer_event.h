#pragma once

#include <cstdint>

namespace gazeora {

// What a phone's touch asks of the PC's mouse. The numbers are on the wire (pointer_message.h).
// Click is a press and release in one message, so a lost datagram can't leave a button held.
enum class PointerAction : uint8_t { Move = 1, Press = 2, Release = 3, Scroll = 4, Click = 5 };

enum class PointerButton : uint8_t { Left = 0, Right = 1 };

// One pointer action from the Gazeora app, at a spot in the streamed picture.
struct PointerEvent {
    PointerAction action = PointerAction::Move;
    PointerButton button = PointerButton::Left;
    // Across the picture, 0..1 from its top left.
    float x = 0.0f;
    float y = 0.0f;
    // Scroll only, in Windows' wheel units (120 a notch): positive scrolls up and right.
    int wheelVertical = 0;
    int wheelHorizontal = 0;
};

}  // namespace gazeora
