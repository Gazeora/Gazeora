#include "gazeora/input/gestures/hand_roles.h"

#include <algorithm>

namespace gazeora::input {

HandRoles::Roles HandRoles::Assign(const std::vector<TrackedHand>& hands) {
    Remember(hands);
    Roles roles;
    for (const TrackedHand& hand : RightFirst(hands)) {
        if (!roles.pointer && IsRight(hand)) {
            roles.pointer = hand;
        } else if (!roles.left) {
            roles.left = hand;  // a left hand, or a second one taken for right
        }
    }
    if (roles.pointer) {
        roles.newPointer = roles.pointer->id != m_pointerId;
        m_pointerId = roles.pointer->id;
    }
    return roles;
}

void HandRoles::Remember(const std::vector<TrackedHand>& hands) {
    std::map<int, float> kept;
    for (const TrackedHand& hand : hands) {
        float guess = hand.landmarks.RightHand();
        auto known = m_rightHand.find(hand.id);
        kept[hand.id] =
            known == m_rightHand.end()
                ? guess
                : known->second + (guess - known->second) * m_thresholds.handednessBlend;
    }
    m_rightHand = std::move(kept);
}

std::vector<TrackedHand> HandRoles::RightFirst(const std::vector<TrackedHand>& hands) {
    std::vector<TrackedHand> ordered = hands;
    std::sort(ordered.begin(), ordered.end(), [](const TrackedHand& a, const TrackedHand& b) {
        return a.landmarks.At(HandJoint::Wrist).x < b.landmarks.At(HandJoint::Wrist).x;
    });
    return ordered;
}

}  // namespace gazeora::input
