#pragma once

#include "gazeora/input/gestures/fold_thresholds.h"
#include "gazeora/input/models/hand_landmarks.h"

namespace gazeora::input {

// Whether the thumb and index fingertips touch, picture by picture: near each other in the
// picture, in palm lengths. (The model's 3D joints put them 0.3 to 0.5 apart in pinches as
// often as not.) A fist brings them close too, so the thumb has to be away from the index
// knuckle and the index tip away from the palm. Between the pinched and released lines it
// keeps what it was, so it doesn't flicker.
class PinchReader {
public:
    explicit PinchReader(const FoldThresholds& thresholds) : m_thresholds(thresholds) {}

    bool Read(const HandLandmarks& hand);
    // Whether the fingertips were apart past the released line in the last hand read.
    bool WideOpen() const { return m_wideOpen; }
    void Reset() { m_pinched = false; }

private:
    const FoldThresholds& m_thresholds;
    bool m_pinched = false;
    bool m_wideOpen = true;
};

}  // namespace gazeora::input
