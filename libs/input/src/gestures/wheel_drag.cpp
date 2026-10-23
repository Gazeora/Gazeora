#include "gazeora/input/gestures/wheel_drag.h"

#include <cmath>

namespace gazeora::input {

namespace {

// A palm's length of drag (about 8 cm) turns the wheel five notches: fifteen lines in most
// windows.
constexpr float kWheelPerPalm = 600.0f;
// Moves within a hundredth of the palm (about a millimetre) don't scroll.
constexpr float kDeadZonePalms = 0.01f;
// Once a stroke has gone this far, its direction picks the axis it scrolls along.
constexpr float kAxisPalms = 0.05f;
// Still this long, the fingertip has finished a stroke.
constexpr double kStrokePauseSeconds = 0.3;

}  // namespace

void WheelDrag::Start(Vec2 palms, double now) {
    m_start = m_anchor = palms;
    m_movedAt = now;
    m_axis.reset();
    m_verticalRemainder = m_horizontalRemainder = 0.0f;
}

WheelDrag::Turn WheelDrag::Drag(Vec2 palms, double now) {
    Vec2 step = StepFrom(palms);
    if (step.Length() > 0.0f) {
        m_movedAt = now;
    } else if (now - m_movedAt > kStrokePauseSeconds) {
        m_start = m_anchor;  // a new stroke picks its way afresh
        m_axis.reset();
    }
    Vec2 gone = m_anchor - m_start;
    if (!m_axis && gone.Length() >= kAxisPalms) {
        m_axis = std::abs(gone.y) >= std::abs(gone.x) ? Axis::Vertical : Axis::Horizontal;
        step = gone;  // what went by while the way was picked scrolls too
    }
    Turn turn;
    if (m_axis == Axis::Vertical) {
        turn.vertical = Whole(step.y * kWheelPerPalm, m_verticalRemainder);
    } else if (m_axis == Axis::Horizontal) {
        turn.horizontal = Whole(-step.x * kWheelPerPalm, m_horizontalRemainder);
    }
    return turn;
}

Vec2 WheelDrag::StepFrom(Vec2 palms) {
    Vec2 away = palms - m_anchor;
    float distance = away.Length();
    if (distance <= kDeadZonePalms) {
        return {};
    }
    Vec2 step = away * (1.0f - kDeadZonePalms / distance);
    m_anchor = m_anchor + step;
    return step;
}

int WheelDrag::Whole(float amount, float& remainder) {
    float total = amount + remainder;
    int whole = int(std::trunc(total));
    remainder = total - float(whole);
    return whole;
}

}  // namespace gazeora::input
