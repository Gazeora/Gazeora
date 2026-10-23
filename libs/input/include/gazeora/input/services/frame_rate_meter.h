#pragma once

namespace gazeora::input {

// Pictures per second, averaged over the last second or so.
class FrameRateMeter {
public:
    void Tick(double now);
    float FramesPerSecond() const { return m_framesPerSecond; }

private:
    double m_windowStart = -1.0;
    int m_frames = 0;
    float m_framesPerSecond = 0.0f;
};

}  // namespace gazeora::input
