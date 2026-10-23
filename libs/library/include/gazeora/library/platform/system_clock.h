#pragma once

#include <ctime>

#include "gazeora/library/interfaces/clock.h"

namespace gazeora {

// The real wall clock.
class SystemClock final : public IClock {
public:
    int64_t NowUnixSeconds() const override { return int64_t(std::time(nullptr)); }
};

}  // namespace gazeora
