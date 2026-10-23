#include "gazeora/input/tracking/roi_stabilizer.h"

#include <cmath>

namespace gazeora::input {

namespace {

// The region is twice the hand's size, so these leave the hand well inside it.
constexpr float kMoveFraction = 0.05f;  // of the region's size
constexpr float kResizeFraction = 0.1f;
constexpr float kMaxTurnCosine = 0.9945f;  // 6 degrees

}  // namespace

HandRoi RoiStabilizer::Update(const HandRoi& next) {
    if (!m_kept || MovedAway(*m_kept, next)) {
        m_kept = next;
    }
    return *m_kept;
}

bool RoiStabilizer::MovedAway(const HandRoi& kept, const HandRoi& next) {
    float moved = (next.Center() - kept.Center()).Length() / kept.Size();
    float resized = std::abs(next.Size() / kept.Size() - 1.0f);
    float turnCosine = next.Up().Dot(kept.Up());
    return moved >= kMoveFraction || resized >= kResizeFraction || turnCosine <= kMaxTurnCosine;
}

}  // namespace gazeora::input
