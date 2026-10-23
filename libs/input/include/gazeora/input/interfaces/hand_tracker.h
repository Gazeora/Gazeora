#pragma once

#include <vector>

#include "gazeora/input/models/camera_frame.h"
#include "gazeora/input/models/tracked_hand.h"

namespace gazeora::input {

// Finds the hands in each camera picture, following each from picture to picture.
class IHandTracker {
public:
    static constexpr size_t kMaxHands = 2;

    virtual ~IHandTracker() = default;

    // Up to kMaxHands hands; none if no hand is in view.
    virtual std::vector<TrackedHand> Track(const CameraFrame& frame) = 0;
};

}  // namespace gazeora::input
