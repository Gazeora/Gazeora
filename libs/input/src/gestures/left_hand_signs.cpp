#include "gazeora/input/gestures/left_hand_signs.h"

namespace gazeora::input {

LeftSign LeftHandSigns::Update(const HandLandmarks* left, double now) {
    if (!left) {
        if (now - m_lastSeen > m_thresholds.lostHandSeconds) {
            Reset();
        }
        return m_sign;
    }
    m_lastSeen = now;
    LeftSign shown = Shown(*left);
    if (shown != m_shown) {
        m_shown = shown;
        m_shownSince = now;
    }
    if (shown == LeftSign::None || now - m_shownSince >= Delay(shown)) {
        m_sign = shown;
    } else if (m_sign != shown) {
        m_sign = LeftSign::None;  // a sign ends at once; the next counts once it has lasted
    }
    return m_sign;
}

void LeftHandSigns::Reset() {
    m_pinch.Reset();
    m_poses.Reset();
    m_shown = m_sign = LeftSign::None;
}

LeftSign LeftHandSigns::Shown(const HandLandmarks& left) {
    bool pinched = m_pinch.Read(left);
    // The index up alone: two fingers up, or the whole hand open, mean nothing.
    bool indexUp = m_poses.Read(left) == HandPose::Pointing && !m_poses.Open();
    if (pinched) {
        return LeftSign::Pinch;
    }
    return indexUp ? LeftSign::IndexUp : LeftSign::None;
}

double LeftHandSigns::Delay(LeftSign shown) const {
    return shown == LeftSign::Pinch ? m_thresholds.scrollOnSeconds : m_thresholds.holdDelaySeconds;
}

}  // namespace gazeora::input
