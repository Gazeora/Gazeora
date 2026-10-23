#pragma once

namespace gazeora::input {

// A clock that only moves forward, injected so tests can step time.
class IMonotonicClock {
public:
    virtual ~IMonotonicClock() = default;

    virtual double NowSeconds() const = 0;
};

}  // namespace gazeora::input
