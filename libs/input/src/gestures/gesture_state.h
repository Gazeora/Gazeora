#pragma once

#include <memory>

#include "gazeora/input/gestures/fold_thresholds.h"
#include "gazeora/input/gestures/hand_pose.h"

namespace gazeora::input {

// State pattern: one mode of HandGestureDetector.
class GestureState {
public:
    virtual ~GestureState() = default;

    // The state to move to after this picture, or nothing to stay. Sets `clicked` when the
    // move completes a click.
    virtual std::unique_ptr<GestureState> Next(const HandSigns& signs, double now,
                                               bool& clicked) const = 0;
    virtual GestureMode Mode() const = 0;
};

}  // namespace gazeora::input
