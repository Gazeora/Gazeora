#include <gtest/gtest.h>

#include "gazeora/input/gestures/pinch_reader.h"
#include "gazeora/input/gestures/wheel_drag.h"
#include "gazeora/input/models/hand_gesture_settings.h"
#include "gazeora/input/services/hand_gesture_controller.h"
#include "support/fakes.h"

namespace gazeora::input {
namespace {

using testing::Hands;

constexpr double kFrame = 1.0 / 30;
const Vec3 kUp{0.0f, -1.0f, 0.0f};
// The fake palm is 102 pixels long: this is half of it.
constexpr float kHalfAPalm = 51.0f;
// The left hand, on the camera's right.
const Vec3 kLeftSide{300.0f, 0.0f, 0.0f};
const CameraFrame kFrameOf1000Px(1000, 1000, std::vector<uint8_t>(1000 * 1000 * 4));

// Drags evenly from `from` to `to`, in palms, over `steps` pictures; the wheel turned in all.
WheelDrag::Turn Drag(WheelDrag& drag, Vec2 from, Vec2 to, double& now, int steps = 15) {
    WheelDrag::Turn total;
    for (int step = 1; step <= steps; ++step) {
        now += kFrame;
        WheelDrag::Turn turn = drag.Drag(from + (to - from) * (float(step) / steps), now);
        total.vertical += turn.vertical;
        total.horizontal += turn.horizontal;
    }
    return total;
}

TEST(WheelDragTest, MovingUpScrollsDownLikeATouchScreen) {
    WheelDrag drag;
    double now = 0.0;
    drag.Start({}, now);
    WheelDrag::Turn turn = Drag(drag, {}, {0.0f, -0.5f}, now);
    EXPECT_NEAR(turn.vertical, -0.49f * 600.0f, 2.0f);  // less the dead zone
    EXPECT_EQ(turn.horizontal, 0);
}

TEST(WheelDragTest, MovingToTheUsersRightScrollsLeft) {
    WheelDrag drag;
    double now = 0.0;
    drag.Start({}, now);
    WheelDrag::Turn turn = Drag(drag, {}, {0.5f, 0.0f}, now);
    EXPECT_LT(turn.horizontal, -250);
    EXPECT_EQ(turn.vertical, 0);
}

TEST(WheelDragTest, AStrokeScrollsOnlyTheWayItWentFirst) {
    WheelDrag drag;
    double now = 0.0;
    drag.Start({}, now);
    WheelDrag::Turn turn = Drag(drag, {}, {0.15f, 0.5f}, now);  // mostly down, a little sideways
    EXPECT_GT(turn.vertical, 250);
    EXPECT_EQ(turn.horizontal, 0);
}

TEST(WheelDragTest, APauseLetsTheNextStrokeGoTheOtherWay) {
    WheelDrag drag;
    double now = 0.0;
    drag.Start({}, now);
    Drag(drag, {}, {0.0f, 0.5f}, now);
    Drag(drag, {0.0f, 0.5f}, {0.0f, 0.5f}, now);  // half a second still
    WheelDrag::Turn turn = Drag(drag, {0.0f, 0.5f}, {0.5f, 0.5f}, now);
    EXPECT_LT(turn.horizontal, -250);
    EXPECT_EQ(turn.vertical, 0);
}

TEST(PinchReaderTest, ReadsAPinchAndKeepsItUntilTheFingertipsPart) {
    FoldThresholds thresholds;
    PinchReader reader(thresholds);
    HandLandmarks pointing = Hands::Pointing(kUp);
    EXPECT_FALSE(reader.Read(pointing));
    EXPECT_TRUE(reader.Read(Hands::ThumbTo(pointing, 5.0f)));
    EXPECT_TRUE(reader.Read(Hands::ThumbTo(pointing, 30.0f)));  // between the lines
    EXPECT_FALSE(reader.Read(Hands::ThumbTo(pointing, 40.0f)));
}

TEST(PinchReaderTest, AFistIsNoPinch) {
    FoldThresholds thresholds;
    HandLandmarks fist = Hands::Folding(testing::kFolded, testing::kFolded);
    EXPECT_FALSE(PinchReader(thresholds).Read(Hands::ThumbTo(fist, 5.0f)));
}

class TwoHandScrollTest : public ::testing::Test {
protected:
    void Show(const HandLandmarks& right, std::optional<HandLandmarks> left, int frames = 1) {
        for (int i = 0; i < frames; ++i) {
            if (left) {
                m_tracker.Add(right, *left);
            } else {
                m_tracker.Add(right);
            }
            m_now += kFrame;
            m_snapshot = m_controller.Process(kFrameOf1000Px, m_now);
        }
    }
    // The right index finger moved up by half a palm over half a second, the left hand
    // `left`.
    void MoveUp(std::optional<HandLandmarks> left) {
        for (int frame = 1; frame <= 15; ++frame) {
            Show(Hands::Pointing(kUp, {0.0f, -kHalfAPalm * frame / 15, 0.0f}), left);
        }
        Show(Hands::Pointing(kUp, {0.0f, -kHalfAPalm, 0.0f}), left, 5);
    }

    HandLandmarks m_pinched = Hands::AsLeft(Hands::ThumbTo(Hands::Pointing(kUp, kLeftSide)));
    // The left hand saying nothing: a fist.
    HandLandmarks m_open =
        Hands::AsLeft(Hands::Folding(testing::kFolded, testing::kFolded, kLeftSide));
    testing::ScriptedTracker m_tracker;
    testing::FakePointer m_pointer;
    HandGestureController m_controller{m_tracker, m_pointer, HandGestureSettings::kDefaultSpeed};
    TrackingSnapshot m_snapshot;
    double m_now = 0.0;
};

TEST_F(TwoHandScrollTest, ALeftPinchTurnsTheFingersMovesIntoScrolling) {
    Show(Hands::Pointing(kUp), m_open, 30);
    Show(Hands::Pointing(kUp), m_pinched, 5);
    MoveUp(m_pinched);
    EXPECT_EQ(m_snapshot.mode, GestureMode::Scrolling);
    EXPECT_TRUE(m_snapshot.otherHand);
    EXPECT_LT(m_pointer.scrolledUp, -200);
    EXPECT_NEAR(m_pointer.position.x, 0.5f, 1e-4f);
    EXPECT_NEAR(m_pointer.position.y, 0.5f, 1e-4f);
}

TEST_F(TwoHandScrollTest, WithoutThePinchTheFingerMovesThePointer) {
    Show(Hands::Pointing(kUp), m_open, 30);
    MoveUp(m_open);
    EXPECT_EQ(m_pointer.scrolledUp, 0);
    EXPECT_LT(m_pointer.position.y, 0.4f);
}

TEST_F(TwoHandScrollTest, AfterScrollingThePointerGoesOnFromWhereItWas) {
    Show(Hands::Pointing(kUp), m_open, 30);
    Show(Hands::Pointing(kUp), m_pinched, 5);
    MoveUp(m_pinched);
    Show(Hands::Pointing(kUp, {0.0f, -kHalfAPalm, 0.0f}), m_open, 30);  // parted
    EXPECT_NEAR(m_pointer.position.y, 0.5f, 0.01f);
    EXPECT_NE(m_snapshot.mode, GestureMode::Scrolling);
}

TEST_F(TwoHandScrollTest, TheLeftHandLeavingStopsScrolling) {
    Show(Hands::Pointing(kUp), m_pinched, 10);
    Show(Hands::Pointing(kUp), std::nullopt, 15);  // half a second
    EXPECT_NE(m_snapshot.mode, GestureMode::Scrolling);
}

// The right hand pinching moves the pointer as ever: scrolling takes the left hand.
TEST_F(TwoHandScrollTest, TheRightHandPinchingDoesntScroll) {
    Show(Hands::Pointing(kUp), std::nullopt, 30);
    Show(Hands::ThumbTo(Hands::Pointing(kUp)), std::nullopt, 10);
    MoveUp(std::nullopt);
    EXPECT_EQ(m_pointer.scrolledUp, 0);
}

}  // namespace
}  // namespace gazeora::input
