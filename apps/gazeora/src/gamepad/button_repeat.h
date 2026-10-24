#pragma once

namespace gazeora::app {

// A held button that fires again and again, like a held key: once when pressed, then after a
// pause, steadily until let go.
class ButtonRepeat {
public:
    static constexpr double kDefaultDelaySeconds = 0.4;
    static constexpr double kDefaultRateSeconds = 0.09;

    ButtonRepeat() = default;
    // Throws std::invalid_argument unless both are above 0.
    ButtonRepeat(double delaySeconds, double rateSeconds);

    // Once a frame: whether it fires now.
    bool Fire(bool held, double nowSeconds);

private:
    double m_delay = kDefaultDelaySeconds;
    double m_rate = kDefaultRateSeconds;
    bool m_held = false;
    double m_nextAt = 0.0;
};

}  // namespace gazeora::app
