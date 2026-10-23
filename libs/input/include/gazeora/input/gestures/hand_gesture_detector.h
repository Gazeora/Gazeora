#pragma once

#include <memory>

#include "gazeora/input/gestures/finger_gesture.h"
#include "gazeora/input/gestures/fold_thresholds.h"
#include "gazeora/input/gestures/hand_pose.h"

namespace gazeora::input {

class GestureState;

// State pattern: turns what the hands say into pointer modes and clicks. The right index
// fingertip moves the pointer; the left index up holds it, and then folding the right index
// and opening it again clicks; the left hand pinching scrolls instead.
class HandGestureDetector {
public:
    explicit HandGestureDetector(FoldThresholds thresholds = FoldThresholds());
    ~HandGestureDetector();

    // What the hands say in a picture taken at `now` seconds.
    FingerGesture Update(const HandSigns& signs, double now);
    // The hand was lost: start over, resting.
    void Reset();

private:
    FoldThresholds m_thresholds;
    std::unique_ptr<GestureState> m_state;
};

}  // namespace gazeora::input
