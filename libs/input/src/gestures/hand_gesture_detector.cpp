#include "gazeora/input/gestures/hand_gesture_detector.h"

#include "gestures/gesture_states.h"

namespace gazeora::input {

HandGestureDetector::HandGestureDetector(FoldThresholds thresholds)
    : m_thresholds(thresholds), m_state(std::make_unique<RestingState>(m_thresholds)) {}

HandGestureDetector::~HandGestureDetector() = default;

FingerGesture HandGestureDetector::Update(const HandSigns& signs, double now) {
    FingerGesture gesture;
    if (auto next = m_state->Next(signs, now, gesture.clicked)) {
        m_state = std::move(next);
    }
    gesture.mode = m_state->Mode();
    return gesture;
}

void HandGestureDetector::Reset() { m_state = std::make_unique<RestingState>(m_thresholds); }

}  // namespace gazeora::input
