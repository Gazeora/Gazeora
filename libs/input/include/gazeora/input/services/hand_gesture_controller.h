#pragma once

#include <optional>
#include <vector>

#include "gazeora/input/gestures/air_touch_pointer.h"
#include "gazeora/input/gestures/hand_gesture_detector.h"
#include "gazeora/input/gestures/hand_pose_reader.h"
#include "gazeora/input/gestures/hand_roles.h"
#include "gazeora/input/gestures/landmark_smoother.h"
#include "gazeora/input/gestures/left_hand_signs.h"
#include "gazeora/input/gestures/open_hand_trigger.h"
#include "gazeora/input/gestures/wheel_drag.h"
#include "gazeora/input/interfaces/hand_tracker.h"
#include "gazeora/input/interfaces/pointer_device.h"
#include "gazeora/input/models/tracking_snapshot.h"

namespace gazeora::input {

// Hand gestures to pointer, like a touch screen you don't touch: the pointer goes where the
// right index fingertip is (AirTouchPointer). The left hand only signals (LeftHandSigns): its
// index up holds the pointer, and then folding the right index and opening it again clicks;
// its pinch turns the right fingertip's moves into scrolling. The mouse can take over at any
// time; held open a moment, the right hand puts the pointer back under the fingertip. One
// camera picture at a time.
class HandGestureController {
public:
    HandGestureController(IHandTracker& tracker, IPointerDevice& pointer, float speed);

    void SetSpeed(float speed) { m_touch.SetSpeed(speed); }
    // Tracks the hands in `frame`, taken at `now` seconds, and moves, clicks or scrolls the
    // pointer. The snapshot has everything but the frame rate and message.
    TrackingSnapshot Process(const CameraFrame& frame, double now);

private:
    TrackingSnapshot Follow(const TrackedHand& right, const std::optional<TrackedHand>& left,
                            LeftSign sign, const CameraFrame& frame, double now);
    TrackingSnapshot NoHand(const std::optional<TrackedHand>& left, const CameraFrame& frame);
    void Steer(const HandLandmarks& hand, LeftSign sign, double now, TrackingSnapshot& snapshot);
    // Moves, scrolls or clicks for the gesture; the pointer stays put otherwise.
    void Act(const FingerGesture& gesture, const HandLandmarks& hand, double now);
    // The fingertip's moves as wheel turns.
    void Scroll(const HandLandmarks& hand, double now);
    void Aim(const HandLandmarks& hand, double now);
    // The hand was held open: the pointer back under the fingertip, held or not.
    void Realign();
    void MovePointer(Vec2 position);
    // The mouse moved the pointer since we last did: steer on from where it is.
    bool MouseMovedPointer() const;
    void LoseHand();
    static std::array<Vec2, kHandJointCount> Joints(const HandLandmarks& hand,
                                                    const CameraFrame& frame);

    IHandTracker& m_tracker;
    IPointerDevice& m_pointer;
    FoldThresholds m_thresholds;
    HandRoles m_roles{m_thresholds};
    LeftHandSigns m_leftSigns{m_thresholds};
    LandmarkSmoother m_smoother;
    AirTouchPointer m_touch;
    HandPoseReader m_poses;
    HandGestureDetector m_gestures;
    OpenHandTrigger m_open{m_thresholds};
    WheelDrag m_drag;
    std::optional<float> m_scrollPalmLength;  // while scrolling: fixed, so leaning in doesn't
    std::optional<Vec2> m_lastMove;
    std::optional<TrackingSnapshot> m_lastTracked;  // shown again through a dropped picture
    double m_lastSeen = 0.0;
    bool m_stilled = false;  // the pointer held still: steering goes on from where it is
    int m_clickCount = 0;
};

}  // namespace gazeora::input
