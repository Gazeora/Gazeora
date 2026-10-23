#include "gazeora/input/tracking/roi_calculator.h"

#include <gtest/gtest.h>

#include "support/fakes.h"

namespace gazeora::input {
namespace {

TEST(RoiCalculatorTest, PointsUpFromWristToKnuckles) {
    std::array<Landmark, kHandJointCount> joints{};
    joints.fill({125.0f, 200.0f, 0.0f});
    joints[int(HandJoint::Wrist)] = {100.0f, 200.0f, 0.0f};
    for (HandJoint knuckle : {HandJoint::IndexMcp, HandJoint::MiddleMcp, HandJoint::RingMcp}) {
        joints[int(knuckle)] = {150.0f, 200.0f, 0.0f};  // the hand points right
    }
    HandRoi roi = RoiCalculator().FromLandmarks(HandLandmarks(joints, 1.0f));
    EXPECT_NEAR(roi.Up().x, 1.0f, 1e-5f);
    EXPECT_NEAR(roi.Up().y, 0.0f, 1e-5f);
}

TEST(RoiCalculatorTest, FingertipsDontMoveTheRegion) {
    RoiCalculator rois;
    HandLandmarks hand = testing::Hands::Pointing({0.0f, -1.0f, 0.0f});
    std::array<Landmark, kHandJointCount> joints = hand.All();
    for (HandJoint end : {HandJoint::IndexDip, HandJoint::IndexTip, HandJoint::MiddleDip,
                          HandJoint::MiddleTip, HandJoint::RingTip, HandJoint::PinkyTip}) {
        joints[int(end)].x += 40.0f;  // the fingertips curl and sway
        joints[int(end)].y += 25.0f;
    }
    HandRoi straight = rois.FromLandmarks(hand);
    HandRoi turned = rois.FromLandmarks(HandLandmarks(joints, 1.0f));
    EXPECT_FLOAT_EQ(turned.Center().x, straight.Center().x);
    EXPECT_FLOAT_EQ(turned.Center().y, straight.Center().y);
    EXPECT_FLOAT_EQ(turned.Size(), straight.Size());
}

TEST(RoiCalculatorTest, ReachesFromThePalmToTheFingertips) {
    std::array<Vec2, Palm::kKeypointCount> keypoints;
    keypoints.fill({100.0f, 150.0f});
    keypoints[Palm::kWristKeypoint] = {100.0f, 200.0f};
    keypoints[Palm::kMiddleFingerKeypoint] = {100.0f, 100.0f};
    HandRoi roi = RoiCalculator().FromPalm(Palm(1.0f, keypoints));
    EXPECT_NEAR(roi.Size(), 300.0f, 1e-3f);
    EXPECT_LT(roi.Center().y, 150.0f);  // above the palm, toward the fingers
}

}  // namespace
}  // namespace gazeora::input
