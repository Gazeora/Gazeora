#pragma once

#include <optional>

#include "gazeora/input/gestures/fold_thresholds.h"
#include "gazeora/input/gestures/hand_pose.h"
#include "gazeora/input/gestures/hand_pose_reader.h"
#include "gazeora/input/gestures/pinch_reader.h"
#include "gazeora/input/models/hand_landmarks.h"

namespace gazeora::input {

// The only two things the left hand says: its thumb and index fingertips pinched for
// FoldThresholds::scrollOnSeconds (scroll), or its index finger up, alone, for
// holdDelaySeconds (hold the pointer, ready to click). Anything else it does means nothing.
// Out of view for longer than a dropped picture, it says nothing.
class LeftHandSigns {
public:
    explicit LeftHandSigns(const FoldThresholds& thresholds)
        : m_thresholds(thresholds), m_pinch(thresholds), m_poses(thresholds) {}

    // `left` is the left hand in the picture taken at `now`, if it's in view.
    LeftSign Update(const HandLandmarks* left, double now);
    void Reset();

private:
    // What the hand shows in this picture, before it has lasted.
    LeftSign Shown(const HandLandmarks& left);
    // How long `shown` has to last before it counts.
    double Delay(LeftSign shown) const;

    const FoldThresholds& m_thresholds;
    PinchReader m_pinch;
    HandPoseReader m_poses;
    LeftSign m_shown = LeftSign::None;
    double m_shownSince = 0.0;
    double m_lastSeen = 0.0;
    LeftSign m_sign = LeftSign::None;
};

}  // namespace gazeora::input
