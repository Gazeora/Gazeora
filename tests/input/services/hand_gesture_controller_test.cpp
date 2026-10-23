#include "gazeora/input/services/hand_gesture_controller.h"

#include <gtest/gtest.h>

#include "support/fakes.h"

namespace gazeora::input {
namespace {

using testing::Hands;
using testing::kFolded;
using testing::kHalfFolded;
using testing::kStraight;

constexpr float kSpeed = 10.0f;  // an air box 1.2 palms wide
constexpr double kFrame = 1.0 / 30;
const Vec3 kUp{0.0f, -1.0f, 0.0f};
// The fake palm is 102 pixels long and the index finger 85: turned this way, its tip goes a
// fifth of the box to the camera's left, the user's right.
const Vec3 kTurnedRight = Vec3{-0.288f, -0.958f, 0.0f}.Normalized();
// A fifth of the box, in pixels.
constexpr float kFifthOfTheBox = 24.48f;
// Pointing at the camera, its tip shows 0.4 of the finger's length below where it does up.
const Vec3 kAtTheCamera{0.0f, -0.6f, -0.8f};
// A deliberate fold or opening takes a few pictures.
constexpr int kPoseFrames = 4;
// The left index has to be up a fifth of a second to hold.
constexpr int kHoldFrames = 8;
const CameraFrame kFrameOf1000Px(1000, 1000, std::vector<uint8_t>(1000 * 1000 * 4));
// The left hand, on the camera's right.
const Vec3 kLeftSide{300.0f, 0.0f, 0.0f};

class HandGestureControllerTest : public ::testing::Test {
protected:
    TrackingSnapshot Show(std::optional<HandLandmarks> right, int frames = 1,
                          std::optional<HandLandmarks> left = std::nullopt) {
        TrackingSnapshot snapshot;
        for (int i = 0; i < frames; ++i) {
            if (right && left) {
                m_tracker.Add(*right, *left);
            } else if (left) {
                m_tracker.pictures.push_back({{testing::ScriptedTracker::kLeftHand, *left}});
            } else {
                m_tracker.Add(right);
            }
            m_now += kFrame;
            snapshot = m_controller.Process(kFrameOf1000Px, m_now);
        }
        return snapshot;
    }
    // The left index raised to hold, then the right index folded halfway.
    void HoldAndFold() {
        Show(Hands::Pointing(kUp), kHoldFrames, m_leftIndexUp);
        Show(m_rightFolded, kPoseFrames, m_leftIndexUp);
    }

    const HandLandmarks m_leftIndexUp = Hands::AsLeft(Hands::Pointing(kUp, kLeftSide));
    const HandLandmarks m_rightFolded = Hands::Folding(kHalfFolded, kFolded);
    testing::ScriptedTracker m_tracker;
    testing::FakePointer m_pointer;
    HandGestureController m_controller{m_tracker, m_pointer, kSpeed};
    double m_now = 0.0;
};

TEST_F(HandGestureControllerTest, TurningTheFingerMovesThePointer) {
    Show(Hands::Pointing(kUp), 30);
    TrackingSnapshot snapshot = Show(Hands::Pointing(kTurnedRight), 30);
    EXPECT_EQ(snapshot.mode, GestureMode::Steering);
    EXPECT_NEAR(m_pointer.position.x, 0.7f, 0.03f);
    EXPECT_NEAR(m_pointer.position.y, 0.5f, 0.03f);  // the tip drops a little as it turns
}

TEST_F(HandGestureControllerTest, MovingTheHandPutsThePointerWhereTheFingertipIs) {
    Show(Hands::Pointing(kUp), 30);
    // A fifth of the box up and to the user's right (the camera's left).
    Show(Hands::Pointing(kUp, {-kFifthOfTheBox, -kFifthOfTheBox, 0.0f}), 30);
    EXPECT_NEAR(m_pointer.position.x, 0.7f, 0.03f);
    EXPECT_NEAR(m_pointer.position.y, 0.3f, 0.03f);
}

TEST_F(HandGestureControllerTest, RaisingTheFingerMovesThePointerUp) {
    Show(Hands::Pointing(kAtTheCamera), 30);
    Show(Hands::Pointing(kUp), 30);
    EXPECT_NEAR(m_pointer.position.y, 0.22f, 0.03f);  // a third of a palm up
    EXPECT_NEAR(m_pointer.position.x, 0.5f, 0.001f);
}

TEST_F(HandGestureControllerTest, PointingDownIsNotAFold) {
    Show(Hands::Pointing(kUp), 10);
    EXPECT_EQ(Show(Hands::Pointing({0.0f, 1.0f, 0.0f}), 10).mode, GestureMode::Steering);
    EXPECT_GT(m_pointer.position.y, 0.9f);  // the fingertip went down two fingers' lengths
}

// The session passes the setting on with every picture.
TEST_F(HandGestureControllerTest, KeepsMovingWhenTheSpeedIsSetEveryPicture) {
    for (int frame = 0; frame < 60; ++frame) {
        m_controller.SetSpeed(kSpeed);
        Show(Hands::Pointing(frame < 30 ? kUp : kTurnedRight));
    }
    EXPECT_GT(m_pointer.position.x, 0.6f);
}

TEST_F(HandGestureControllerTest, TheLeftIndexUpHoldsThePointer) {
    Show(Hands::Pointing(kUp), 30);
    Show(Hands::Pointing(kUp), kHoldFrames, m_leftIndexUp);
    TrackingSnapshot holding =
        Show(Hands::Pointing(kUp, {-kFifthOfTheBox, 0.0f, 0.0f}), 30, m_leftIndexUp);
    EXPECT_EQ(holding.mode, GestureMode::Holding);
    EXPECT_NEAR(m_pointer.position.x, 0.5f, 0.02f);
    // Lowered, the pointer goes on from where it held, not under the moved finger.
    Show(Hands::Pointing(kUp, {-kFifthOfTheBox, 0.0f, 0.0f}), 30);
    EXPECT_NEAR(m_pointer.position.x, 0.5f, 0.02f);
}

TEST_F(HandGestureControllerTest, FoldingTheRightIndexWhileHoldingAndOpeningItClicks) {
    HoldAndFold();
    TrackingSnapshot opened = Show(Hands::Pointing(kUp), kPoseFrames, m_leftIndexUp);
    EXPECT_EQ(m_pointer.clicks, 1);
    EXPECT_EQ(opened.clicks, 1);
}

TEST_F(HandGestureControllerTest, WithoutTheLeftIndexFoldingNeverClicks) {
    Show(Hands::Pointing(kUp), 10);
    Show(m_rightFolded, kPoseFrames);
    Show(Hands::Pointing(kUp), 10);
    EXPECT_EQ(m_pointer.clicks, 0);
}

TEST_F(HandGestureControllerTest, ABriefDropoutKeepsTheFold) {
    HoldAndFold();
    EXPECT_EQ(Show(std::nullopt).state, TrackingState::Tracking);  // not shown as lost
    Show(Hands::Pointing(kUp), kPoseFrames, m_leftIndexUp);
    EXPECT_EQ(m_pointer.clicks, 1);
}

TEST_F(HandGestureControllerTest, LosingTheHandCancelsAFold) {
    HoldAndFold();
    EXPECT_EQ(Show(std::nullopt, 15).state, TrackingState::NoHand);  // half a second
    Show(Hands::Pointing(kUp), kPoseFrames, m_leftIndexUp);
    EXPECT_EQ(m_pointer.clicks, 0);
}

TEST_F(HandGestureControllerTest, TheLeftHandAloneNeverMovesThePointer) {
    TrackingSnapshot snapshot = Show(std::nullopt, 30, Hands::AsLeft(Hands::Pointing(kUp)));
    Show(std::nullopt, 30, Hands::AsLeft(Hands::Pointing(kUp, {0.0f, -kFifthOfTheBox, 0.0f})));
    EXPECT_TRUE(m_pointer.moves.empty());
    EXPECT_EQ(snapshot.state, TrackingState::NoHand);
    EXPECT_TRUE(snapshot.otherHand);
}

TEST_F(HandGestureControllerTest, TheLeftHandsOtherGesturesMeanNothing) {
    Show(Hands::Pointing(kUp), 10);
    for (const HandLandmarks& left :
         {Hands::Open(), Hands::Folding(kStraight, kStraight), Hands::Folding(kFolded, kFolded)}) {
        TrackingSnapshot snapshot = Show(Hands::Pointing(kUp), 15, Hands::AsLeft(left));
        EXPECT_EQ(snapshot.mode, GestureMode::Steering);
    }
}

TEST_F(HandGestureControllerTest, AHandHeldOpenPutsThePointerUnderTheFingertip) {
    Show(Hands::Pointing(kUp), 30);
    m_pointer.position = {0.2f, 0.8f};  // moved with the mouse
    Show(Hands::Open(), 5);             // a moment open: nothing yet
    EXPECT_NEAR(m_pointer.position.x, 0.2f, 1e-4f);
    Show(Hands::Open(), 15);  // over half a second
    EXPECT_NEAR(m_pointer.position.x, 0.5f, 0.02f);
    EXPECT_NEAR(m_pointer.position.y, 0.5f, 0.02f);
}

TEST_F(HandGestureControllerTest, TheMouseCanTakeOver) {
    Show(Hands::Pointing(kUp), 10);
    m_pointer.position = {0.2f, 0.8f};  // moved with the mouse
    Show(Hands::Pointing(kUp), 10);
    EXPECT_NEAR(m_pointer.position.x, 0.2f, 1e-4f);
    EXPECT_NEAR(m_pointer.position.y, 0.8f, 1e-4f);
}

}  // namespace
}  // namespace gazeora::input
