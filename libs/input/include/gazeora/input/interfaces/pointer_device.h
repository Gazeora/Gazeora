#pragma once

#include "gazeora/input/models/vec2.h"

namespace gazeora::input {

// The system's mouse pointer. Positions run from 0 to 1 across the main screen.
class IPointerDevice {
public:
    virtual ~IPointerDevice() = default;

    virtual Vec2 Position() const = 0;
    // The main screen's width over its height.
    virtual float ScreenAspect() const = 0;
    virtual void MoveTo(Vec2 position) = 0;
    virtual void Click() = 0;
    // Turns the wheels, in Windows' units (120 a notch): `vertical` positive scrolls up,
    // `horizontal` positive scrolls right.
    virtual void Scroll(int vertical, int horizontal) = 0;
};

}  // namespace gazeora::input
