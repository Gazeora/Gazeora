#include "gazeora/input/gestures/finger_posture.h"

namespace gazeora::input {

bool FingerPosture::Update(float straightness, float reach) {
    if (straightness < m_thresholds.folded || reach < m_thresholds.foldedReach) {
        m_straight = false;
    } else if (straightness > m_thresholds.extended && reach > m_thresholds.extendedReach) {
        m_straight = true;
    } else if (!m_straight) {
        // First sight, between the lines: whichever it's nearer.
        m_straight = straightness > (m_thresholds.folded + m_thresholds.extended) / 2;
    }
    return *m_straight;
}

}  // namespace gazeora::input
