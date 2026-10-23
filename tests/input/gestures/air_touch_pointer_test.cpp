#include "gazeora/input/gestures/air_touch_pointer.h"

#include <gtest/gtest.h>

#include <random>

#include "gazeora/input/gestures/finger_straightness.h"
#include "gazeora/input/gestures/fingertip.h"
#include "gazeora/input/gestures/one_euro_filter.h"
#include "support/fakes.h"

namespace gazeora::input {
namespace {

constexpr float kSpeed = 10.0f;  // a box 1.2 palms wide
constexpr float kPalm = 100.0f;  // pixels
constexpr double kFrame = 1.0 / 30;
const Vec2 kStart{300.0f, 200.0f};

class AirTouchPointerTest : public ::testing::Test {
protected:
    AirTouchPointerTest() { m_touch.StartAt({0.5f, 0.5f}); }

    // Holds the fingertip at `tip` for a second; the pointer's last position.
    Vec2 Hold(Vec2 tip) { return Glide(tip, tip, 30); }
    // Moves the fingertip evenly from `from` to `to` over `frames` pictures.
    Vec2 Glide(Vec2 from, Vec2 to, int frames) {
        Vec2 position;
        for (int frame = 1; frame <= frames; ++frame) {
            m_now += kFrame;
            position = m_touch.Steer(from + (to - from) * (float(frame) / frames), kPalm, m_now);
        }
        return position;
    }

    AirTouchPointer m_touch{kSpeed};
    double m_now = 0.0;
};

// The pointer stays this close to where the fingertip is in the box.
constexpr float kFineTune = 0.03f;

TEST_F(AirTouchPointerTest, AQuickMoveLandsWhereTheFingertipIs) {
    Hold(kStart);
    Vec2 position = Hold(kStart + Vec2{24.0f, 0.0f});  // a fifth of the box to the right
    EXPECT_NEAR(position.x, 0.7f, kFineTune + 0.005f);
    EXPECT_NEAR(position.y, 0.5f, 1e-4f);
}

TEST_F(AirTouchPointerTest, ASlowMoveLandsWhereTheFingertipIsToo) {
    Hold(kStart);
    Glide(kStart, kStart + Vec2{24.0f, 0.0f}, 60);  // over two seconds
    EXPECT_NEAR(Hold(kStart + Vec2{24.0f, 0.0f}).x, 0.7f, kFineTune + 0.005f);
}

TEST_F(AirTouchPointerTest, ASmallSlowMoveIsFinerThanTheFingertip) {
    Hold(kStart);
    Glide(kStart, kStart + Vec2{4.0f, 0.0f}, 60);  // 0.03 of the box, over two seconds
    float moved = Hold(kStart + Vec2{4.0f, 0.0f}).x - 0.5f;
    EXPECT_GT(moved, 0.0f);
    EXPECT_LT(moved, 0.02f);  // the fingertip went 0.033
}

TEST_F(AirTouchPointerTest, TheMouseTakesOverUntilTheFingertipMoves) {
    Hold(kStart);
    m_touch.TakeOver({0.1f, 0.9f});
    Vec2 still = Hold(kStart);
    EXPECT_NEAR(still.x, 0.1f, 1e-4f);
    EXPECT_NEAR(still.y, 0.9f, 1e-4f);
    Vec2 moved = Hold(kStart + Vec2{24.0f, 0.0f});
    EXPECT_NEAR(moved.x, 0.7f, kFineTune + 0.01f);
    EXPECT_NEAR(moved.y, 0.5f, kFineTune + 0.01f);
}

TEST_F(AirTouchPointerTest, UpAndDownWorkJustLikeSideways) {
    Hold(kStart);
    float across = Hold(kStart + Vec2{24.0f, 0.0f}).x - 0.5f;
    m_touch.StartAt({0.5f, 0.5f});
    Hold(kStart);
    float up = 0.5f - Hold(kStart + Vec2{0.0f, -24.0f}).y;
    EXPECT_NEAR(up, across, 1e-4f);
}

TEST_F(AirTouchPointerTest, CoversAsManyPixelsUpAsSideways) {
    m_touch.SetScreenAspect(16.0f / 9);
    m_touch.StartAt({0.5f, 0.5f});
    Hold(kStart);
    Vec2 position = Hold(kStart + Vec2{10.0f, -10.0f});
    // As many pixels up as across is 16/9 as much of the height.
    EXPECT_NEAR((position.x - 0.5f) * 16.0f / 9, 0.5f - position.y, 0.002f);
}

TEST_F(AirTouchPointerTest, AHandNearerTheCameraDoesntJump) {
    Hold(kStart);
    m_now += kFrame;
    Vec2 position = m_touch.Steer(kStart, kPalm * 1.3f, m_now);  // leaned in
    EXPECT_NEAR(position.x, 0.5f, 1e-4f);
}

// The fingertip noise left after the landmark smoother: about a pixel, with a 4 pixel jump
// now and then.
TEST_F(AirTouchPointerTest, StaysStillThroughTrackingNoise) {
    Hold(kStart);
    std::mt19937 random(7);
    std::normal_distribution<float> noise(0.0f, 1.0f);
    Vec2 position;
    for (int frame = 0; frame < 300; ++frame) {  // 10 seconds
        m_now += kFrame;
        float jump = frame % 50 == 25 ? 4.0f : 0.0f;
        position = m_touch.Steer(kStart + Vec2{noise(random) + jump, noise(random)}, kPalm, m_now);
    }
    EXPECT_NEAR(position.x, 0.5f, 0.005f);
    EXPECT_NEAR(position.y, 0.5f, 0.005f);
}

TEST_F(AirTouchPointerTest, ReachesTheVeryEdgeEvenMovingSlowly) {
    Hold(kStart);
    // Up past the box's top, slowly: fine-tuning mustn't leave the pointer short of the edge.
    Glide(kStart, kStart + Vec2{0.0f, -70.0f}, 90);
    EXPECT_FLOAT_EQ(Hold(kStart + Vec2{0.0f, -70.0f}).y, 0.0f);
    Glide(kStart + Vec2{0.0f, -70.0f}, kStart + Vec2{0.0f, 70.0f}, 90);
    EXPECT_FLOAT_EQ(Hold(kStart + Vec2{0.0f, 70.0f}).y, 1.0f);
}

TEST_F(AirTouchPointerTest, StopsAtTheScreensEdgeAndComesBackAtOnce) {
    Hold(kStart);
    EXPECT_FLOAT_EQ(Hold(kStart + Vec2{80.0f, 0.0f}).x, 1.0f);
    EXPECT_LT(Hold(kStart + Vec2{60.0f, 0.0f}).x, 0.9f);
}

TEST(FingertipTest, MirrorsLikeASelfieView) {
    HandLandmarks hand = testing::Hands::Pointing({0.0f, -1.0f, 0.0f});
    EXPECT_FLOAT_EQ(Fingertip::Position(hand).x, -hand.At(HandJoint::IndexTip).x);
    EXPECT_NEAR(Fingertip::PalmLength(hand), 102.0f, 1.0f);
}

TEST(FingerStraightnessTest, StraightWhicheverWayItPoints) {
    for (Vec3 direction : {Vec3{0.0f, -1.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.6f, 0.8f, 0.0f},
                           Vec3{0.0f, 0.0f, -1.0f}}) {
        HandLandmarks hand = testing::Hands::Pointing(direction);
        EXPECT_NEAR(FingerStraightness::Of(hand, Finger::Index), 1.0f, 1e-4f);
    }
}

TEST(FingerStraightnessTest, FallsAsTheFingerFolds) {
    auto straightness = [](float bend) {
        return FingerStraightness::Of(testing::Hands::Folding(bend, 0.0f), Finger::Index);
    };
    EXPECT_NEAR(straightness(testing::kSlightlyBent), 0.86f, 0.02f);
    EXPECT_NEAR(straightness(testing::kHalfFolded), 0.75f, 0.02f);
    EXPECT_LT(straightness(testing::kFolded), 0.4f);
}

TEST(OneEuroFilterTest, SmoothsJitterAtRest) {
    OneEuroFilter<Vec2> filter;
    filter.Filter({0.5f, 0.5f}, 0.0);
    Vec2 jittered = filter.Filter({0.51f, 0.5f}, kFrame);
    EXPECT_LT(jittered.x - 0.5f, 0.005f);
}

}  // namespace
}  // namespace gazeora::input
