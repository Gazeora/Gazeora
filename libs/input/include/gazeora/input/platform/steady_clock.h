#pragma once

#include <chrono>

#include "gazeora/input/interfaces/monotonic_clock.h"

namespace gazeora::input {

// The real monotonic clock.
class SteadyClock final : public IMonotonicClock {
public:
    double NowSeconds() const override {
        using Seconds = std::chrono::duration<double>;
        return std::chrono::duration_cast<Seconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
};

}  // namespace gazeora::input
