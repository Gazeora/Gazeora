#include "gazeora/input/services/frame_rate_meter.h"

namespace gazeora::input {

namespace {

constexpr double kWindowSeconds = 1.0;

}  // namespace

void FrameRateMeter::Tick(double now) {
    if (m_windowStart < 0.0) {
        m_windowStart = now;
    }
    ++m_frames;
    double elapsed = now - m_windowStart;
    if (elapsed >= kWindowSeconds) {
        m_framesPerSecond = float(m_frames / elapsed);
        m_frames = 0;
        m_windowStart = now;
    }
}

}  // namespace gazeora::input
