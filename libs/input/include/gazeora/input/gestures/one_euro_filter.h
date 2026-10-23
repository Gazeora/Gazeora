#pragma once

#include "gazeora/input/gestures/one_euro_tuning.h"

namespace gazeora::input {

// The 1-euro filter (Casiez et al., CHI 2012): smooths hard while a value barely moves, so
// tracking jitter doesn't shake the pointer, and less as it speeds up, so it doesn't lag.
// `Vector` is Vec2 or Vec3.
template <typename Vector>
class OneEuroFilter {
public:
    explicit OneEuroFilter(OneEuroTuning tuning = OneEuroTuning()) : m_tuning(tuning) {}

    Vector Filter(Vector value, double now) {
        if (!m_primed) {
            m_value = value;
            m_velocity = Vector();
            m_lastTime = now;
            m_primed = true;
            return m_value;
        }
        float seconds = float(now - m_lastTime);
        if (seconds <= 0.0f) {
            return m_value;
        }
        Vector velocity = (value - m_value) * (1.0f / seconds);
        m_velocity = Blend(m_velocity, velocity, Weight(m_tuning.speedCutoffHz, seconds));
        float cutoff = m_tuning.minCutoffHz + m_tuning.beta * m_velocity.Length();
        m_value = Blend(m_value, value, Weight(cutoff, seconds));
        m_lastTime = now;
        return m_value;
    }
    void Reset() { m_primed = false; }
    // How fast the value is moving, per second, smoothed: it stays up until the filtered value
    // has caught up with a quick move.
    Vector Velocity() const { return m_velocity; }

private:
    static constexpr float kTwoPi = 6.28318530718f;

    static Vector Blend(Vector from, Vector to, float weight) {
        return from + (to - from) * weight;
    }
    // How much of a new sample to take, for a low-pass at `cutoffHz` after `seconds`.
    static float Weight(float cutoffHz, float seconds) {
        float timeConstant = 1.0f / (kTwoPi * cutoffHz);
        return 1.0f / (1.0f + timeConstant / seconds);
    }

    OneEuroTuning m_tuning;
    bool m_primed = false;
    Vector m_value{};
    Vector m_velocity{};
    double m_lastTime = 0.0;
};

}  // namespace gazeora::input
