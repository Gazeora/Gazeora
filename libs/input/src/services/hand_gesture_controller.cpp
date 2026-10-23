#include "gazeora/input/services/hand_gesture_controller.h"

#include <algorithm>

#include "gazeora/input/gestures/fingertip.h"

namespace gazeora::input {

namespace {

// Farther than this from where we put it, the mouse moved the pointer (a few pixels).
constexpr float kMouseMoveDistance = 0.003f;

}  // namespace

HandGestureController::HandGestureController(IHandTracker& tracker, IPointerDevice& pointer,
                                             float speed)
    : m_tracker(tracker), m_pointer(pointer), m_touch(speed) {}

TrackingSnapshot HandGestureController::Process(const CameraFrame& frame, double now) {
    HandRoles::Roles roles = m_roles.Assign(m_tracker.Track(frame));
    LeftSign sign = m_leftSigns.Update(roles.left ? &roles.left->landmarks : nullptr, now);
    if (roles.pointer) {
        if (roles.newPointer) {
            LoseHand();  // a different hand: start it fresh
        }
        return Follow(*roles.pointer, roles.left, sign, frame, now);
    }
    if (m_lastTracked && now - m_lastSeen <= m_thresholds.lostHandSeconds) {
        return *m_lastTracked;  // a dropped picture: the hand is still there
    }
    LoseHand();
    return NoHand(roles.left, frame);
}

TrackingSnapshot HandGestureController::Follow(const TrackedHand& right,
                                               const std::optional<TrackedHand>& left,
                                               LeftSign sign, const CameraFrame& frame,
                                               double now) {
    TrackingSnapshot snapshot;
    snapshot.cameraAspect = float(frame.Width()) / frame.Height();
    m_lastSeen = now;
    HandLandmarks hand = m_smoother.Smooth(right.landmarks, now);
    Steer(hand, sign, now, snapshot);
    snapshot.joints = Joints(hand, frame);
    if (left) {
        snapshot.otherHand = Joints(left->landmarks, frame);
    }
    snapshot.state = TrackingState::Tracking;
    snapshot.clicks = m_clickCount;
    m_lastTracked = snapshot;
    return snapshot;
}

TrackingSnapshot HandGestureController::NoHand(const std::optional<TrackedHand>& left,
                                               const CameraFrame& frame) {
    TrackingSnapshot snapshot;
    snapshot.cameraAspect = float(frame.Width()) / frame.Height();
    snapshot.state = TrackingState::NoHand;
    snapshot.clicks = m_clickCount;
    if (left) {
        snapshot.otherHand = Joints(left->landmarks, frame);
    }
    return snapshot;
}

void HandGestureController::Steer(const HandLandmarks& hand, LeftSign sign, double now,
                                  TrackingSnapshot& snapshot) {
    HandPose pose = m_poses.Read(hand);
    if (m_open.Update(m_poses.Open(), now)) {
        Realign();
    }
    bool indexUp = pose == HandPose::Pointing || pose == HandPose::TwoUp;
    FingerGesture gesture = m_gestures.Update({sign, indexUp}, now);
    Act(gesture, hand, now);
    snapshot.mode = gesture.mode;
    snapshot.pointerHeld = gesture.PointerHeld();
    snapshot.handOpen = m_poses.Open();
}

void HandGestureController::Act(const FingerGesture& gesture, const HandLandmarks& hand,
                                double now) {
    if (gesture.mode == GestureMode::Scrolling) {
        Scroll(hand, now);
    } else {
        m_scrollPalmLength.reset();
    }
    if (gesture.mode == GestureMode::Steering) {
        if (m_stilled) {
            m_touch.StartAt(m_pointer.Position());  // the finger moved while it was still
            m_stilled = false;
        }
        Aim(hand, now);
    } else {
        m_stilled = true;
    }
    if (gesture.clicked) {
        m_pointer.Click();
        ++m_clickCount;
    }
}

void HandGestureController::Scroll(const HandLandmarks& hand, double now) {
    if (!m_scrollPalmLength) {
        m_scrollPalmLength = std::max(Fingertip::PalmLength(hand), 1.0f);
        m_drag.Start(Fingertip::Position(hand) * (1.0f / *m_scrollPalmLength), now);
        return;
    }
    WheelDrag::Turn turn =
        m_drag.Drag(Fingertip::Position(hand) * (1.0f / *m_scrollPalmLength), now);
    if (turn.vertical != 0 || turn.horizontal != 0) {
        m_pointer.Scroll(turn.vertical, turn.horizontal);
    }
}

void HandGestureController::Aim(const HandLandmarks& hand, double now) {
    if (!m_touch.Started()) {
        m_touch.SetScreenAspect(m_pointer.ScreenAspect());
        m_touch.StartAt(m_pointer.Position());
    } else if (MouseMovedPointer()) {
        m_touch.TakeOver(m_pointer.Position());
    }
    Vec2 position = m_touch.Steer(Fingertip::Position(hand), Fingertip::PalmLength(hand), now);
    if (!m_lastMove || position.x != m_lastMove->x || position.y != m_lastMove->y) {
        MovePointer(position);
    }
}

void HandGestureController::Realign() {
    if (std::optional<Vec2> under = m_touch.Realign()) {
        MovePointer(*under);
    }
}

void HandGestureController::MovePointer(Vec2 position) {
    m_pointer.MoveTo(position);
    m_lastMove = position;
}

bool HandGestureController::MouseMovedPointer() const {
    return m_lastMove && (m_pointer.Position() - *m_lastMove).Length() > kMouseMoveDistance;
}

void HandGestureController::LoseHand() {
    m_smoother.Reset();
    m_touch.Reset();
    m_poses.Reset();
    m_gestures.Reset();
    m_open.Reset();
    m_scrollPalmLength.reset();
    m_lastMove.reset();
    m_lastTracked.reset();
    m_stilled = false;
}

std::array<Vec2, kHandJointCount> HandGestureController::Joints(const HandLandmarks& hand,
                                                                const CameraFrame& frame) {
    std::array<Vec2, kHandJointCount> joints;
    for (int i = 0; i < kHandJointCount; ++i) {
        const Landmark& joint = hand.All()[i];
        joints[i] = {1.0f - joint.x / frame.Width(), joint.y / frame.Height()};
    }
    return joints;
}

}  // namespace gazeora::input
