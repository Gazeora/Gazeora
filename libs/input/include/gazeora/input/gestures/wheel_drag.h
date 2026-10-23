#pragma once

#include <optional>

#include "gazeora/input/models/vec2.h"

namespace gazeora::input {

// Wheel turns for a fingertip dragged like a finger on a touch screen: the page follows it, so
// moving up scrolls down and moving to the user's right scrolls left. Once a stroke has clearly
// gone one way, it scrolls only that way (vertically or sideways); a pause ends the stroke, and
// the next picks its way afresh. The fingertip has to leave a small dead zone first, so
// tracking noise doesn't scroll.
class WheelDrag {
public:
    // Wheel amounts, in the units Windows uses (120 a notch).
    struct Turn {
        int vertical = 0;    // positive scrolls up
        int horizontal = 0;  // positive scrolls right
    };

    // Starts a drag with the fingertip at `palms` (Fingertip::Position over the palm's length)
    // at `now` seconds.
    void Start(Vec2 palms, double now);
    // The wheel turn for the fingertip moved to `palms` at `now`.
    Turn Drag(Vec2 palms, double now);

private:
    enum class Axis { Vertical, Horizontal };

    // How far the fingertip went past the dead zone; moves the zone along.
    Vec2 StepFrom(Vec2 palms);
    // Whole wheel units for `amount`, keeping the fraction for the next turn.
    static int Whole(float amount, float& remainder);

    Vec2 m_start;
    Vec2 m_anchor;
    std::optional<Axis> m_axis;
    float m_verticalRemainder = 0.0f;
    float m_horizontalRemainder = 0.0f;
    double m_movedAt = 0.0;  // when the fingertip last left the dead zone
};

}  // namespace gazeora::input
