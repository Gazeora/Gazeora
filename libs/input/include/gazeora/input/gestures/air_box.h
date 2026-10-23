#pragma once

#include "gazeora/input/models/vec2.h"

namespace gazeora::input {

// The box in the air that stands for the screen, like a touch screen you don't touch: a
// fingertip at its top left points at the screen's top left. Measured in palm lengths in the
// camera's (mirrored) view, so it doesn't depend on how near the camera the hand is; as tall
// as the screen is, for its width, so a move covers as many pixels up as sideways.
class AirBox {
public:
    // A box `widthPalms` wide on a screen `screenAspect` (width over height), placed so the
    // fingertip at `tip` points where the pointer is, `screenPosition` (0 to 1 across).
    AirBox(Vec2 tip, Vec2 screenPosition, float widthPalms, float screenAspect);

    // Where on the screen `tip` points: 0 to 1 inside the box, beyond outside it.
    Vec2 ToScreen(Vec2 tip) const;
    // Screens moved for a fingertip move of `palms`.
    Vec2 ScreensFor(Vec2 palms) const;
    // Slides the box so `tip` points at the screen's nearest edge, if it's past one: moving
    // back moves away from the edge at once.
    void KeepOnScreen(Vec2 tip);
    // Changes the width, keeping where `tip` points.
    void Resize(float widthPalms, Vec2 tip);

private:
    Vec2 m_topLeft;
    Vec2 m_size;
};

}  // namespace gazeora::input
