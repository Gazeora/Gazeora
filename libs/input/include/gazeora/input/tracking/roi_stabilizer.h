#pragma once

#include <optional>

#include "gazeora/input/models/hand_roi.h"

namespace gazeora::input {

// Keeps the landmark model looking at the same region until the hand really moves. Placing
// the region from last picture's joints passes their noise on to the next crop, which shakes
// the joints more; holding it still breaks that loop.
class RoiStabilizer {
public:
    // The region to use: the last one kept, unless `next` moved, grew or turned past a
    // small threshold.
    HandRoi Update(const HandRoi& next);
    void Reset() { m_kept.reset(); }

private:
    static bool MovedAway(const HandRoi& kept, const HandRoi& next);

    std::optional<HandRoi> m_kept;
};

}  // namespace gazeora::input
