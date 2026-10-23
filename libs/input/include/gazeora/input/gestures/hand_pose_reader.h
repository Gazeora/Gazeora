#pragma once

#include "gazeora/input/gestures/finger.h"
#include "gazeora/input/gestures/finger_posture.h"
#include "gazeora/input/gestures/hand_pose.h"
#include "gazeora/input/models/hand_landmarks.h"

namespace gazeora::input {

// Reads the hand's pose from its fingers, picture by picture.
class HandPoseReader {
public:
    explicit HandPoseReader(FoldThresholds thresholds = FoldThresholds())
        : m_thresholds(thresholds),
          m_index(thresholds),
          m_middle(thresholds),
          m_ring(thresholds),
          m_pinky(thresholds) {}

    HandPose Read(const HandLandmarks& hand);
    // Whether every finger was up in the last hand read. Seen from the knuckles the model
    // draws folded fingers straight now and then, so this is kept apart from the pose: only
    // an open hand held a while (OpenHandTrigger) means anything.
    bool Open() const { return m_open; }
    // The hand was lost: read the next one fresh.
    void Reset();

private:
    static bool Straight(FingerPosture& posture, const HandLandmarks& hand, Finger finger);
    // Whether the middle fingertip is beside the index one in the picture
    // (FoldThresholds::togetherPalms): the model often draws a folded middle finger straight,
    // but rarely up there.
    bool TipsTogether(const HandLandmarks& hand) const;

    FoldThresholds m_thresholds;
    FingerPosture m_index;
    FingerPosture m_middle;
    FingerPosture m_ring;
    FingerPosture m_pinky;
    bool m_open = false;
};

}  // namespace gazeora::input
