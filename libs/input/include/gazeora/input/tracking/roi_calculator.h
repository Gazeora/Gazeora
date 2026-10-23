#pragma once

#include <vector>

#include "gazeora/input/models/hand_landmarks.h"
#include "gazeora/input/models/hand_roi.h"
#include "gazeora/input/models/palm.h"

namespace gazeora::input {

// Where the landmark model should look: a square around the hand, turned so the fingers
// point up in the crop, the way the model was trained.
class RoiCalculator {
public:
    // Around a palm just detected: the fingers aren't found yet, so the square is generous.
    HandRoi FromPalm(const Palm& palm) const;
    // Around last picture's palm and finger bases, for following the hand into this one.
    HandRoi FromLandmarks(const HandLandmarks& hand) const;

private:
    // The points' bounding box in the turned frame, moved toward the fingers by `shift` of
    // its height and grown to a square `scale` times its longer side.
    static HandRoi Around(const std::vector<Vec2>& points, Vec2 base, Vec2 tip, float scale,
                          float shift);
};

}  // namespace gazeora::input
