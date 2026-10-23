#include "gazeora/input/gestures/open_hand_trigger.h"

namespace gazeora::input {

bool OpenHandTrigger::Update(bool open, double now) {
    if (!open) {
        Reset();
        return false;
    }
    if (!m_openSince) {
        m_openSince = now;
    }
    if (m_fired || now - *m_openSince < m_thresholds.openHoldSeconds) {
        return false;
    }
    m_fired = true;
    return true;
}

void OpenHandTrigger::Reset() {
    m_openSince.reset();
    m_fired = false;
}

}  // namespace gazeora::input
