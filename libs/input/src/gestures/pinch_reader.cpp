#include "gazeora/input/gestures/pinch_reader.h"

#include <iterator>

namespace gazeora::input {

namespace {

constexpr HandJoint kPalmJoints[] = {HandJoint::Wrist, HandJoint::IndexMcp, HandJoint::MiddleMcp,
                                     HandJoint::RingMcp, HandJoint::PinkyMcp};

Vec2 PalmMiddle(const HandLandmarks& hand) {
    Vec2 sum;
    for (HandJoint joint : kPalmJoints) {
        sum = sum + hand.At(joint).Xy();
    }
    return sum * (1.0f / float(std::size(kPalmJoints)));
}

}  // namespace

bool PinchReader::Read(const HandLandmarks& hand) {
    float palm = (hand.At(HandJoint::MiddleMcp).Xy() - hand.At(HandJoint::Wrist).Xy()).Length();
    if (palm <= 0.0f) {
        return m_pinched;
    }
    auto apart = [&](Vec2 a, Vec2 b) { return (a - b).Length() / palm; };
    Vec2 thumb = hand.At(HandJoint::ThumbTip).Xy(), tip = hand.At(HandJoint::IndexTip).Xy();
    float tips = apart(thumb, tip);
    bool notAFist = apart(thumb, hand.At(HandJoint::IndexMcp).Xy()) > m_thresholds.fistThumbPalms &&
                    apart(tip, PalmMiddle(hand)) > m_thresholds.fistTipPalms;
    if (tips < m_thresholds.pinchedPalms && notAFist) {
        m_pinched = true;
    } else if (tips > m_thresholds.releasedPalms || !notAFist) {
        m_pinched = false;
    }
    m_wideOpen = tips > m_thresholds.releasedPalms;
    return m_pinched;
}

}  // namespace gazeora::input
