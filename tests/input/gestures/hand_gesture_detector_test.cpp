#include "gazeora/input/gestures/hand_gesture_detector.h"

#include <gtest/gtest.h>

#include "gazeora/input/gestures/hand_pose_reader.h"
#include "gazeora/input/gestures/open_hand_trigger.h"
#include "support/fakes.h"

namespace gazeora::input {
namespace {

constexpr double kFrame = 1.0 / 30;

class HandGestureDetectorTest : public ::testing::Test {
protected:
    // Shows `left` and the right index up or not for `frames` pictures; the last picture's
    // gesture, with any click seen.
    FingerGesture Show(LeftSign left, bool indexUp, int frames = 1) {
        FingerGesture last;
        bool clicked = false;
        for (int i = 0; i < frames; ++i) {
            m_now += kFrame;
            last = m_detector.Update({left, indexUp}, m_now);
            clicked |= last.clicked;
        }
        last.clicked = clicked;
        return last;
    }

    HandGestureDetector m_detector;
    double m_now = 0.0;
};

constexpr bool kUp = true, kDown = false;

TEST_F(HandGestureDetectorTest, WithTheLeftHandSayingNothingTheIndexSteers) {
    EXPECT_EQ(Show(LeftSign::None, kUp).mode, GestureMode::Steering);
    // Pointing at the camera, the index looks folded: it mustn't stop the pointer or click.
    FingerGesture folded = Show(LeftSign::None, kDown, 5);
    EXPECT_EQ(folded.mode, GestureMode::Steering);
    EXPECT_FALSE(Show(LeftSign::None, kUp, 5).clicked);
}

TEST_F(HandGestureDetectorTest, TheLeftIndexUpHolds) {
    Show(LeftSign::None, kUp, 5);
    FingerGesture holding = Show(LeftSign::IndexUp, kUp);
    EXPECT_EQ(holding.mode, GestureMode::Holding);
    EXPECT_TRUE(holding.PointerHeld());
}

TEST_F(HandGestureDetectorTest, FoldingTheRightIndexAndOpeningItWhileHoldingClicks) {
    Show(LeftSign::IndexUp, kUp, 5);
    EXPECT_EQ(Show(LeftSign::IndexUp, kDown, 4).mode, GestureMode::Folded);
    FingerGesture opened = Show(LeftSign::IndexUp, kUp, 2);
    EXPECT_TRUE(opened.clicked);
    EXPECT_EQ(opened.mode, GestureMode::Holding);
}

TEST_F(HandGestureDetectorTest, ClicksAgainAndAgainWhileHolding) {
    Show(LeftSign::IndexUp, kUp, 5);
    int clicks = 0;
    for (int click = 0; click < 3; ++click) {
        Show(LeftSign::IndexUp, kDown, 4);
        clicks += Show(LeftSign::IndexUp, kUp, 4).clicked;
    }
    EXPECT_EQ(clicks, 3);
}

// The left index raised with the right one curled: opening it isn't a click.
TEST_F(HandGestureDetectorTest, TheRightIndexHasToBeUpFirst) {
    Show(LeftSign::None, kDown, 5);
    Show(LeftSign::IndexUp, kDown, 4);
    EXPECT_FALSE(Show(LeftSign::IndexUp, kUp, 2).clicked);
}

TEST_F(HandGestureDetectorTest, LoweringTheLeftIndexMidClickSteersWithoutClicking) {
    Show(LeftSign::IndexUp, kUp, 5);
    Show(LeftSign::IndexUp, kDown, 4);
    FingerGesture lowered = Show(LeftSign::None, kUp, 2);
    EXPECT_EQ(lowered.mode, GestureMode::Steering);
    EXPECT_FALSE(lowered.clicked);
}

TEST_F(HandGestureDetectorTest, HoldingTheFoldTooLongIsNoClick) {
    Show(LeftSign::IndexUp, kUp, 5);
    Show(LeftSign::IndexUp, kDown, 45);  // a second and a half
    EXPECT_FALSE(Show(LeftSign::IndexUp, kUp).clicked);
}

TEST_F(HandGestureDetectorTest, TheLeftPinchScrollsFromAnyMode) {
    EXPECT_EQ(Show(LeftSign::Pinch, kUp).mode, GestureMode::Scrolling);
    EXPECT_EQ(Show(LeftSign::IndexUp, kUp).mode, GestureMode::Holding);
    Show(LeftSign::IndexUp, kDown);
    EXPECT_EQ(Show(LeftSign::Pinch, kUp).mode, GestureMode::Scrolling);
    EXPECT_EQ(Show(LeftSign::None, kUp).mode, GestureMode::Steering);
}

TEST(HandPoseReaderTest, ReadsTheTwoFingers) {
    using testing::Hands;
    HandPoseReader reader;
    EXPECT_EQ(reader.Read(Hands::Folding(testing::kStraight, testing::kFolded)),
              HandPose::Pointing);
    EXPECT_EQ(reader.Read(Hands::Folding(testing::kStraight, testing::kStraight)), HandPose::TwoUp);
    EXPECT_EQ(reader.Read(Hands::Folding(testing::kFolded, testing::kFolded)), HandPose::Fist);
    EXPECT_EQ(reader.Read(Hands::Folding(testing::kFolded, testing::kStraight)), HandPose::Other);
}

TEST(HandPoseReaderTest, AFingerBetweenTheLinesKeepsItsPosture) {
    using testing::Hands;
    HandPoseReader reader;
    reader.Read(Hands::Folding(testing::kStraight, testing::kFolded));
    EXPECT_EQ(reader.Read(Hands::Folding(testing::kSlightlyBent, testing::kSlightlyBent)),
              HandPose::Pointing);
}

TEST(HandPoseReaderTest, AStraightFingerThatDoesntReachOutIsFolded) {
    // The model's guess at a finger hidden behind the others: its joints in a line, but short.
    HandLandmarks hand = testing::Hands::Folding(testing::kStraight, testing::kStraight);
    HandLandmarks::WorldJoints world = hand.AllWorld();
    for (HandJoint joint : {HandJoint::MiddlePip, HandJoint::MiddleDip, HandJoint::MiddleTip}) {
        Vec3 knuckle = world[int(HandJoint::MiddleMcp)];
        world[int(joint)] = knuckle + (world[int(joint)] - knuckle) * 0.4f;
    }
    EXPECT_EQ(HandPoseReader().Read(HandLandmarks(hand.All(), world, 1.0f)), HandPose::Pointing);
}

// Seen from the knuckles, the model often draws a folded middle finger straight, but its tip
// stays away from the index fingertip.
TEST(HandPoseReaderTest, AStraightMiddleFingerAwayFromTheIndexIsntUp) {
    HandLandmarks hand = testing::Hands::Pointing({1.0f, 0.0f, 0.0f}, {}, testing::kStraight);
    EXPECT_EQ(HandPoseReader().Read(hand), HandPose::Pointing);
}

TEST(HandPoseReaderTest, AnOpenHandCountsAsTwoUp) {
    EXPECT_EQ(HandPoseReader().Read(testing::Hands::Open()), HandPose::TwoUp);
}

TEST(HandPoseReaderTest, ReadsAnOpenHand) {
    HandPoseReader reader;
    reader.Read(testing::Hands::Open());
    EXPECT_TRUE(reader.Open());
    reader.Read(testing::Hands::Folding(testing::kStraight, testing::kStraight));
    EXPECT_FALSE(reader.Open());
}

TEST(OpenHandTriggerTest, FiresOnceAfterHalfASecondOpen) {
    FoldThresholds thresholds;
    OpenHandTrigger trigger(thresholds);
    int fired = 0;
    for (int frame = 0; frame < 30; ++frame) {  // a second open
        fired += trigger.Update(true, frame * kFrame);
    }
    EXPECT_EQ(fired, 1);
    trigger.Update(false, 1.0);
    for (int frame = 0; frame < 20; ++frame) {
        fired += trigger.Update(true, 1.1 + frame * kFrame);
    }
    EXPECT_EQ(fired, 2);  // opened anew
}

// Seen from the knuckles, the model draws a fist open for a quarter of a second at times.
TEST(OpenHandTriggerTest, AGlimpseOfAnOpenHandDoesNothing) {
    FoldThresholds thresholds;
    OpenHandTrigger trigger(thresholds);
    bool fired = false;
    for (int frame = 0; frame < 8; ++frame) {
        fired |= trigger.Update(frame % 8 < 7, frame * kFrame);
    }
    EXPECT_FALSE(fired);
}

TEST(HandPoseReaderTest, HalfwayFoldedCountsAsFolded) {
    using testing::Hands;
    HandPoseReader reader;
    reader.Read(Hands::Folding(testing::kStraight, testing::kStraight));
    EXPECT_EQ(reader.Read(Hands::Folding(testing::kHalfFolded, testing::kHalfFolded)),
              HandPose::Fist);
}

}  // namespace
}  // namespace gazeora::input
