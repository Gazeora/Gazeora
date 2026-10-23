// The GestureState modes. They hand over to each other, so they're declared together; each is
// a few lines.
#pragma once

#include "gestures/gesture_state.h"

namespace gazeora::input {

// The state for what the left hand says: steering when it says nothing.
std::unique_ptr<GestureState> StateFor(LeftSign sign, const FoldThresholds& thresholds);

// Before the first picture of a hand: nothing moves until the hands are seen.
class RestingState final : public GestureState {
public:
    explicit RestingState(const FoldThresholds& thresholds) : m_thresholds(thresholds) {}

    std::unique_ptr<GestureState> Next(const HandSigns& signs, double now,
                                       bool& clicked) const override;
    GestureMode Mode() const override { return GestureMode::Resting; }

private:
    const FoldThresholds& m_thresholds;
};

// The left hand saying nothing: the pointer follows the right index fingertip.
class SteeringState final : public GestureState {
public:
    explicit SteeringState(const FoldThresholds& thresholds) : m_thresholds(thresholds) {}

    std::unique_ptr<GestureState> Next(const HandSigns& signs, double now,
                                       bool& clicked) const override;
    GestureMode Mode() const override { return GestureMode::Steering; }

private:
    const FoldThresholds& m_thresholds;
};

// The left index up: the pointer holds. Once the right index has been seen up (`armed`),
// folding it starts a click; raising the left index with the right one curled doesn't.
class HoldingState final : public GestureState {
public:
    HoldingState(const FoldThresholds& thresholds, bool armed)
        : m_thresholds(thresholds), m_armed(armed) {}

    std::unique_ptr<GestureState> Next(const HandSigns& signs, double now,
                                       bool& clicked) const override;
    GestureMode Mode() const override { return GestureMode::Holding; }

private:
    const FoldThresholds& m_thresholds;
    bool m_armed;
};

// The right index folded while holding, since `since`: opening it soon clicks.
class FoldedState final : public GestureState {
public:
    FoldedState(const FoldThresholds& thresholds, double since)
        : m_thresholds(thresholds), m_since(since) {}

    std::unique_ptr<GestureState> Next(const HandSigns& signs, double now,
                                       bool& clicked) const override;
    GestureMode Mode() const override { return GestureMode::Folded; }

private:
    const FoldThresholds& m_thresholds;
    double m_since;
};

// The left hand pinching: the right index fingertip scrolls.
class ScrollingState final : public GestureState {
public:
    explicit ScrollingState(const FoldThresholds& thresholds) : m_thresholds(thresholds) {}

    std::unique_ptr<GestureState> Next(const HandSigns& signs, double now,
                                       bool& clicked) const override;
    GestureMode Mode() const override { return GestureMode::Scrolling; }

private:
    const FoldThresholds& m_thresholds;
};

}  // namespace gazeora::input
