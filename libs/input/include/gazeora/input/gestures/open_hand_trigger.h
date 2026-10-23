#pragma once

#include <optional>

#include "gazeora/input/gestures/fold_thresholds.h"

namespace gazeora::input {

// Fires once when the hand has been open (HandPoseReader::Open) for
// FoldThresholds::openHoldSeconds, and again only after it closes and opens anew.
class OpenHandTrigger {
public:
    explicit OpenHandTrigger(const FoldThresholds& thresholds) : m_thresholds(thresholds) {}

    // True on the picture the open hand has lasted long enough.
    bool Update(bool open, double now);
    void Reset();

private:
    const FoldThresholds& m_thresholds;
    std::optional<double> m_openSince;
    bool m_fired = false;
};

}  // namespace gazeora::input
