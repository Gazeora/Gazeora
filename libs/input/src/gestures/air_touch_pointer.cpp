#include "gazeora/input/gestures/air_touch_pointer.h"

#include <algorithm>

#include "gazeora/input/models/hand_gesture_settings.h"

namespace gazeora::input {

namespace {

// The box's width in palm lengths times the speed: at 5, the default, 2.4 palms (about
// 20 cm) cross the screen.
constexpr float kBoxPalmsTimesSpeed = 12.0f;
// The fingertip still wobbles by a fraction of a pixel after smoothing; moves within a
// hundredth of the palm (about a millimetre) don't move the pointer.
constexpr float kDeadZonePalms = 0.01f;
// The first pictures of a hand just found shift a few pixels as tracking settles (the
// detector's crop gives way to the tracker's): place the box after them.
constexpr int kSettlingPictures = 4;
// Slow moves nudge the pointer finely, up to this far (a fraction of the screen) from where the
// fingertip is in the box...
constexpr float kFineTuneScreens = 0.03f;
// ...and quick moves close the gap by up to this share each picture. A gap past fine-tuning
// reach, from where the mouse put the pointer, closes by as much once the fingertip moves.
constexpr float kPullPerPicture = 0.3f;

// A fingertip at the box's edge, or past it (the box slides along), puts the pointer right on
// the screen's edge, fine-tuning or not: the taskbar and the window's title are there.
constexpr float kEdgeScreens = 0.001f;

Vec2 Clamped(Vec2 position) {
    return {std::clamp(position.x, 0.0f, 1.0f), std::clamp(position.y, 0.0f, 1.0f)};
}

}  // namespace

AirTouchPointer::AirTouchPointer(float speed) { SetSpeed(speed); }

void AirTouchPointer::SetSpeed(float speed) {
    float clamped =
        std::clamp(speed, HandGestureSettings::kMinSpeed, HandGestureSettings::kMaxSpeed);
    m_widthPalms = kBoxPalmsTimesSpeed / clamped;
    if (m_box) {
        m_box->Resize(m_widthPalms, m_anchor);
    }
}

void AirTouchPointer::StartAt(Vec2 position) {
    m_shown = Clamped(position);
    m_following = true;
    m_box.reset();
    m_palmLength.reset();
    m_settled = 0;
}

Vec2 AirTouchPointer::Steer(Vec2 tip, float palmLength, double now) {
    if (!m_palmLength) {
        m_palmLength = std::max(palmLength, 1.0f);
    }
    Vec2 palms = tip * (1.0f / *m_palmLength);
    if (m_settled < kSettlingPictures) {
        Settle(palms, now);
        return *m_shown;
    }
    Vec2 step = StepFrom(m_filter.Filter(m_median.Filter(palms), now));
    if (step.Length() > 0.0f) {
        m_box->KeepOnScreen(m_anchor);
        Nudge(step, m_filter.Velocity().Length());
        m_following = true;
    }
    if (m_following) {
        m_shown = OntoEdges(Clamped(Leashed(*m_shown)));
    }
    return *m_shown;
}

std::optional<Vec2> AirTouchPointer::Realign() {
    if (!m_box) {
        return std::nullopt;
    }
    m_shown = Touched();
    m_following = true;
    return m_shown;
}

void AirTouchPointer::Reset() {
    m_shown.reset();
    m_box.reset();
    m_palmLength.reset();
    m_median.Reset();
    m_filter.Reset();
}

void AirTouchPointer::Settle(Vec2 palms, double now) {
    // Start the filters over from this picture, so none of the settling carries on.
    ++m_settled;
    m_median.Reset();
    m_filter.Reset();
    m_anchor = m_filter.Filter(m_median.Filter(palms), now);
    if (m_settled == kSettlingPictures) {
        m_box.emplace(m_anchor, *m_shown, m_widthPalms, m_screenAspect);
    }
}

void AirTouchPointer::Nudge(Vec2 step, float palmsPerSecond) {
    Vec2 nudged = *m_shown + m_box->ScreensFor(step) * m_precision.Share(palmsPerSecond);
    Vec2 touched = Touched();
    float pull = m_precision.Quickness(palmsPerSecond) * kPullPerPicture;
    m_shown = touched + (nudged - touched) * (1.0f - pull);
}

Vec2 AirTouchPointer::Touched() const { return Clamped(m_box->ToScreen(m_anchor)); }

Vec2 AirTouchPointer::OntoEdges(Vec2 shown) const {
    Vec2 touched = m_box->ToScreen(m_anchor);
    auto onEdge = [](float at) { return at <= kEdgeScreens || at >= 1.0f - kEdgeScreens; };
    Vec2 edge = Clamped(touched);
    return {onEdge(touched.x) ? edge.x : shown.x, onEdge(touched.y) ? edge.y : shown.y};
}

Vec2 AirTouchPointer::Leashed(Vec2 shown) const {
    Vec2 touched = Touched();
    Vec2 offset = shown - touched;
    float distance = offset.Length();
    if (distance <= kFineTuneScreens) {
        return shown;
    }
    float pulled = kFineTuneScreens + (distance - kFineTuneScreens) * (1.0f - kPullPerPicture);
    return touched + offset * (pulled / distance);
}

Vec2 AirTouchPointer::StepFrom(Vec2 smooth) {
    Vec2 away = smooth - m_anchor;
    float distance = away.Length();
    if (distance <= kDeadZonePalms) {
        return {};
    }
    Vec2 step = away * (1.0f - kDeadZonePalms / distance);
    m_anchor = m_anchor + step;
    return step;
}

}  // namespace gazeora::input
