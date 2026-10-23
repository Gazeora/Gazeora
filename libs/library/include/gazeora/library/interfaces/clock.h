#pragma once

#include <cstdint>

namespace gazeora {

// The wall clock, injected so tests can fix the time.
class IClock {
public:
    virtual ~IClock() = default;

    virtual int64_t NowUnixSeconds() const = 0;
};

}  // namespace gazeora
