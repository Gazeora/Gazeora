#pragma once

#include <memory>

#include "gazeora/input/interfaces/hand_tracker.h"

namespace gazeora::input {

// Factory pattern: a fresh tracker for each tracking session. Throws InputError if the
// tracker's models can't be loaded.
class IHandTrackerFactory {
public:
    virtual ~IHandTrackerFactory() = default;

    virtual std::unique_ptr<IHandTracker> Create() = 0;
};

}  // namespace gazeora::input
