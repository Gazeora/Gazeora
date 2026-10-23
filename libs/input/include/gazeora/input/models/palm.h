#pragma once

#include <array>

#include "gazeora/input/models/vec2.h"

namespace gazeora::input {

// A palm found by the detector: its score and 7 keypoints in camera pixels.
class Palm {
public:
    static constexpr int kKeypointCount = 7;
    static constexpr int kWristKeypoint = 0;
    static constexpr int kMiddleFingerKeypoint = 2;

    Palm(float score, std::array<Vec2, kKeypointCount> keypoints)
        : m_score(score), m_keypoints(keypoints) {}

    float Score() const { return m_score; }
    const std::array<Vec2, kKeypointCount>& Keypoints() const { return m_keypoints; }
    // Whether `other` is this same palm, found again from a neighbouring anchor: its wrist
    // nearer this one's than the palm is long.
    bool Overlaps(const Palm& other) const {
        float length = (m_keypoints[kMiddleFingerKeypoint] - m_keypoints[kWristKeypoint]).Length();
        return (other.m_keypoints[kWristKeypoint] - m_keypoints[kWristKeypoint]).Length() < length;
    }

private:
    float m_score;
    std::array<Vec2, kKeypointCount> m_keypoints;
};

}  // namespace gazeora::input
