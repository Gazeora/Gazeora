#include "gazeora/input/gestures/hand_pose_reader.h"

#include "gazeora/input/gestures/finger_reach.h"
#include "gazeora/input/gestures/finger_straightness.h"

namespace gazeora::input {

HandPose HandPoseReader::Read(const HandLandmarks& hand) {
    bool index = Straight(m_index, hand, Finger::Index);
    bool middle = Straight(m_middle, hand, Finger::Middle);
    bool others = Straight(m_ring, hand, Finger::Ring) & Straight(m_pinky, hand, Finger::Pinky);
    m_open = index && middle && others;
    if (index) {
        // Pointing, the model often draws the folded middle finger straight; only beside the
        // index finger is it up.
        return middle && TipsTogether(hand) ? HandPose::TwoUp : HandPose::Pointing;
    }
    return middle ? HandPose::Other : HandPose::Fist;
}

bool HandPoseReader::Straight(FingerPosture& posture, const HandLandmarks& hand, Finger finger) {
    return posture.Update(FingerStraightness::Of(hand, finger), FingerReach::Of(hand, finger));
}

bool HandPoseReader::TipsTogether(const HandLandmarks& hand) const {
    float apart = (hand.At(HandJoint::MiddleTip).Xy() - hand.At(HandJoint::IndexTip).Xy()).Length();
    float palm = (hand.At(HandJoint::MiddleMcp).Xy() - hand.At(HandJoint::Wrist).Xy()).Length();
    return apart < palm * m_thresholds.togetherPalms;
}

void HandPoseReader::Reset() {
    m_index.Reset();
    m_middle.Reset();
    m_ring.Reset();
    m_pinky.Reset();
    m_open = false;
}

}  // namespace gazeora::input
