#pragma once

#include <optional>

#include "gazeora/input/gestures/air_box.h"
#include "gazeora/input/gestures/median_of_three.h"
#include "gazeora/input/gestures/one_euro_filter.h"
#include "gazeora/input/gestures/pointer_precision.h"
#include "gazeora/input/models/vec2.h"

namespace gazeora::input {

// Puts the pointer where the fingertip is in an AirBox, like a touch screen you don't touch,
// fused with a mouse's fine control: near where the fingertip is, slow moves nudge the
// pointer with pointer precision (PointerPrecision), finer than the fingertip goes; the
// pointer is kept close to the fingertip's place, and pulled back to it when the mouse moved
// it away. The fingertip has to leave a small dead zone before anything moves, so tracking
// noise on a still hand never moves the pointer.
class AirTouchPointer {
public:
    explicit AirTouchPointer(float speed);

    // Clamped to HandGestureSettings' range: the faster, the smaller the box.
    void SetSpeed(float speed);
    // The screen's width over its height, for the next box.
    void SetScreenAspect(float aspect) { m_screenAspect = aspect; }
    // Starts with a new hand, placing its box when tracking has settled so the fingertip
    // points at `position` (0 to 1 across the screen), where the pointer is.
    void StartAt(Vec2 position);
    // The pointer was put at `position` (the mouse moved it, or the hand held it): go on from
    // there, keeping the box.
    void TakeOver(Vec2 position) {
        m_shown = position;
        m_following = false;
    }
    // Puts the pointer right where the fingertip is in the box, undoing fine-tuning and where
    // the mouse put it: its new position, if the box is placed yet.
    std::optional<Vec2> Realign();
    bool Started() const { return m_shown.has_value(); }
    // The pointer's position for the fingertip at `tip` (Fingertip::Position) on a hand
    // `palmLength` pixels long, at `now` seconds. Call StartAt first.
    Vec2 Steer(Vec2 tip, float palmLength, double now);
    // The hand was lost: wait for StartAt again.
    void Reset();

private:
    void Settle(Vec2 palms, double now);
    // Moves the pointer by a fingertip `step` at `palmsPerSecond`: slowly, finely, from where
    // it is; quickly, toward where the fingertip is in the box.
    void Nudge(Vec2 step, float palmsPerSecond);
    // Where the fingertip is in the box, on the screen.
    Vec2 Touched() const;
    // `shown`, pulled toward fine-tuning reach of where the fingertip is.
    Vec2 Leashed(Vec2 shown) const;
    // `shown`, put on the screen's edge where the fingertip is on the box's.
    Vec2 OntoEdges(Vec2 shown) const;
    // How far the smoothed fingertip went past the dead zone; moves the zone along.
    Vec2 StepFrom(Vec2 smooth);

    float m_widthPalms = 0.0f;
    float m_screenAspect = 1.0f;
    PointerPrecision m_precision;
    MedianOfThree m_median;
    OneEuroFilter<Vec2> m_filter;
    std::optional<Vec2> m_shown;
    std::optional<AirBox> m_box;
    std::optional<float> m_palmLength;  // fixed for the hand, so leaning in doesn't jump
    Vec2 m_anchor;                      // the middle of the dead zone, in palms
    int m_settled = 0;                  // pictures seen since the hand was found
    bool m_following = false;           // the fingertip moved since the pointer was put somewhere
};

}  // namespace gazeora::input
