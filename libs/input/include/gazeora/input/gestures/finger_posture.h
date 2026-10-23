#pragma once

#include <optional>

#include "gazeora/input/gestures/fold_thresholds.h"

namespace gazeora::input {

// Whether one finger is straight or folded, from its FingerStraightness and FingerReach
// picture by picture: straight takes both, folded either. Between the thresholds it keeps
// what it was, so it doesn't flicker.
class FingerPosture {
public:
    explicit FingerPosture(FoldThresholds thresholds) : m_thresholds(thresholds) {}

    // True if straight.
    bool Update(float straightness, float reach);
    void Reset() { m_straight.reset(); }

private:
    FoldThresholds m_thresholds;
    std::optional<bool> m_straight;
};

}  // namespace gazeora::input
