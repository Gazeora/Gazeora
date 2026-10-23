#pragma once

#include <map>
#include <optional>
#include <vector>

#include "gazeora/input/gestures/fold_thresholds.h"
#include "gazeora/input/models/tracked_hand.h"

namespace gazeora::input {

// Which hand moves the pointer and which signals with the other's gestures: the right hand
// moves it, the left one only signals (LeftHandSigns), by the model's guess at each
// (HandLandmarks::RightHand), averaged over the pictures it's been followed so one odd picture
// doesn't swap them. A left hand alone never moves the pointer. Two hands both taken for
// right: the one further to the user's left is the left hand.
class HandRoles {
public:
    struct Roles {
        std::optional<TrackedHand> pointer;
        std::optional<TrackedHand> left;
        bool newPointer = false;  // a different hand moves the pointer from this picture on
    };

    explicit HandRoles(const FoldThresholds& thresholds) : m_thresholds(thresholds) {}

    Roles Assign(const std::vector<TrackedHand>& hands);

private:
    // Each hand's averaged guess, forgetting hands no longer followed.
    void Remember(const std::vector<TrackedHand>& hands);
    bool IsRight(const TrackedHand& hand) const { return m_rightHand.at(hand.id) >= kEven; }
    // The hand further to the user's right first (the camera faces the user: further left in
    // its picture).
    static std::vector<TrackedHand> RightFirst(const std::vector<TrackedHand>& hands);

    static constexpr float kEven = 0.5f;

    const FoldThresholds& m_thresholds;
    std::map<int, float> m_rightHand;  // by track id
    std::optional<int> m_pointerId;
};

}  // namespace gazeora::input
