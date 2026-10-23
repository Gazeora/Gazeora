#include "gestures/gesture_states.h"

namespace gazeora::input {

std::unique_ptr<GestureState> StateFor(LeftSign sign, const FoldThresholds& thresholds) {
    switch (sign) {
        case LeftSign::IndexUp:
            return std::make_unique<HoldingState>(thresholds, false);
        case LeftSign::Pinch:
            return std::make_unique<ScrollingState>(thresholds);
        case LeftSign::None:
            break;
    }
    return std::make_unique<SteeringState>(thresholds);
}

std::unique_ptr<GestureState> RestingState::Next(const HandSigns& signs, double, bool&) const {
    return StateFor(signs.left, m_thresholds);
}

std::unique_ptr<GestureState> SteeringState::Next(const HandSigns& signs, double, bool&) const {
    return signs.left == LeftSign::None ? nullptr : StateFor(signs.left, m_thresholds);
}

std::unique_ptr<GestureState> HoldingState::Next(const HandSigns& signs, double now, bool&) const {
    if (signs.left != LeftSign::IndexUp) {
        return StateFor(signs.left, m_thresholds);
    }
    if (signs.indexUp) {
        return m_armed ? nullptr : std::make_unique<HoldingState>(m_thresholds, true);
    }
    return m_armed ? std::make_unique<FoldedState>(m_thresholds, now) : nullptr;
}

std::unique_ptr<GestureState> FoldedState::Next(const HandSigns& signs, double now,
                                                bool& clicked) const {
    if (signs.left != LeftSign::IndexUp) {
        return StateFor(signs.left, m_thresholds);  // no click
    }
    if (!signs.indexUp) {
        return nullptr;
    }
    clicked = now - m_since <= m_thresholds.maxClickSeconds;
    return std::make_unique<HoldingState>(m_thresholds, true);
}

std::unique_ptr<GestureState> ScrollingState::Next(const HandSigns& signs, double, bool&) const {
    return signs.left == LeftSign::Pinch ? nullptr : StateFor(signs.left, m_thresholds);
}

}  // namespace gazeora::input
