// The real models on real photos, through ONNX Runtime: needs onnxruntime.dll and models\
// next to the test executable, which the build puts there.
#include "gazeora/input/tracking/mediapipe_hand_tracker.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <random>

#include "gazeora/input/gestures/finger_straightness.h"
#include "gazeora/input/gestures/fold_thresholds.h"
#include "gazeora/input/gestures/hand_pose_reader.h"
#include "gazeora/input/gestures/pinch_reader.h"
#include "gazeora/input/models/hand_gesture_settings.h"
#include "gazeora/input/platform/onnx_runtime_engine.h"
#include "gazeora/input/platform/runtime_files.h"
#include "gazeora/input/services/hand_gesture_controller.h"
#include "gazeora/input/tracking/mediapipe_hand_tracker_factory.h"
#include "support/fakes.h"
#include "support/image_file.h"
#include "support/synthetic_video.h"

namespace gazeora::input {
namespace {

// Where MediaPipe's own pipeline (OpenCV zoo, run in Python) puts the fingertip.
constexpr float kTipTolerancePixels = 5.0f;

class MediaPipeHandTrackerTest : public ::testing::Test {
protected:
    // `frame` with camera-like noise on every channel.
    static CameraFrame Noisy(const CameraFrame& frame, std::mt19937& random,
                             std::normal_distribution<float>& noise) {
        std::vector<uint8_t> pixels;
        pixels.reserve(size_t(frame.Width()) * frame.Height() * CameraFrame::kBytesPerPixel);
        for (int y = 0; y < frame.Height(); ++y) {
            for (int x = 0; x < frame.Width(); ++x) {
                const uint8_t* pixel = frame.Pixel(x, y);
                for (int c = 0; c < CameraFrame::kBytesPerPixel; ++c) {
                    pixels.push_back(uint8_t(std::clamp(pixel[c] + noise(random), 0.0f, 255.0f)));
                }
            }
        }
        return CameraFrame(frame.Width(), frame.Height(), std::move(pixels));
    }

    std::optional<HandLandmarks> TrackPhoto(const wchar_t* name) {
        CameraFrame frame = testing::ImageFile::Load(testing::ImageFile::TestData(name));
        return First(m_tracker->Track(frame));
    }
    static std::optional<HandLandmarks> First(const std::vector<TrackedHand>& hands) {
        return hands.empty() ? std::nullopt : std::optional<HandLandmarks>(hands[0].landmarks);
    }

    OnnxRuntimeEngine m_engine{RuntimeFiles::OnnxRuntimeDll()};
    MediaPipeHandTrackerFactory m_factory{m_engine, RuntimeFiles::PalmModel(),
                                          RuntimeFiles::LandmarkModel()};
    std::unique_ptr<IHandTracker> m_tracker = m_factory.Create();
};

TEST_F(MediaPipeHandTrackerTest, FindsAPointingFinger) {
    std::optional<HandLandmarks> hand = TrackPhoto(L"pointing_up.jpg");
    ASSERT_TRUE(hand);
    EXPECT_GT(hand->Confidence(), 0.9f);
    EXPECT_NEAR(hand->At(HandJoint::IndexTip).x, 168.8f, kTipTolerancePixels);
    EXPECT_NEAR(hand->At(HandJoint::IndexTip).y, 72.1f, kTipTolerancePixels);
    EXPECT_GT(FingerStraightness::Of(*hand, Finger::Index), FoldThresholds().extended);
}

TEST_F(MediaPipeHandTrackerTest, SeesAFoldedFingerInAFist) {
    std::optional<HandLandmarks> hand = TrackPhoto(L"fist.jpg");
    ASSERT_TRUE(hand);
    EXPECT_LT(FingerStraightness::Of(*hand, Finger::Index), FoldThresholds().folded);
}

TEST_F(MediaPipeHandTrackerTest, FollowsTheHandIntoTheNextPicture) {
    std::optional<HandLandmarks> first = TrackPhoto(L"pointing_up.jpg");
    std::optional<HandLandmarks> second = TrackPhoto(L"pointing_up.jpg");
    ASSERT_TRUE(first && second);
    EXPECT_NEAR(second->At(HandJoint::IndexTip).x, first->At(HandJoint::IndexTip).x,
                kTipTolerancePixels);
}

TEST_F(MediaPipeHandTrackerTest, ReadsEachPhotosPose) {
    struct Case {
        const wchar_t* photo;
        HandPose pose;
    };
    for (const Case& one :
         {Case{L"pointing_up.jpg", HandPose::Pointing}, Case{L"victory.jpg", HandPose::TwoUp},
          Case{L"fist.jpg", HandPose::Fist}}) {
        std::optional<HandLandmarks> hand = TrackPhoto(one.photo);
        ASSERT_TRUE(hand);
        EXPECT_EQ(HandPoseReader().Read(*hand), one.pose) << one.photo;
    }
}

TEST_F(MediaPipeHandTrackerTest, FollowsTwoHandsEachUnderItsOwnId) {
    CameraFrame frame = testing::SyntheticVideo::SideBySide(
        testing::ImageFile::Load(testing::ImageFile::TestData(L"pointing_up.jpg")),
        testing::ImageFile::Load(testing::ImageFile::TestData(L"victory.jpg")));
    std::vector<TrackedHand> hands;
    for (int picture = 0; picture < 4; ++picture) {  // the second turns up within a few
        hands = m_tracker->Track(frame);
    }
    ASSERT_EQ(hands.size(), 2u);
    EXPECT_NE(hands[0].id, hands[1].id);
    std::vector<TrackedHand> next = m_tracker->Track(frame);
    ASSERT_EQ(next.size(), 2u);
    EXPECT_EQ(next[0].id, hands[0].id);
    EXPECT_EQ(next[1].id, hands[1].id);
}

// Folding every finger brings the thumb near the index tip, but a fist is no pinch, whichever
// hand makes it.
TEST_F(MediaPipeHandTrackerTest, AFistIsNoPinchOnEitherHand) {
    CameraFrame fist = testing::ImageFile::Load(testing::ImageFile::TestData(L"fist.jpg"));
    CameraFrame blank(fist.Width(), fist.Height(),
                      std::vector<uint8_t>(size_t(fist.Width()) * fist.Height() * 4));
    FoldThresholds thresholds;
    for (const CameraFrame& frame : {fist, testing::SyntheticVideo::SideBySide(blank, fist)}) {
        std::unique_ptr<IHandTracker> tracker = m_factory.Create();
        std::optional<HandLandmarks> hand = First(tracker->Track(frame));
        ASSERT_TRUE(hand);
        EXPECT_FALSE(PinchReader(thresholds).Read(*hand));
    }
}

// A still hand through a noisy camera: the joints should barely move, and never jump.
TEST_F(MediaPipeHandTrackerTest, HoldsAStillHandSteady) {
    CameraFrame photo = testing::ImageFile::Load(testing::ImageFile::TestData(L"pointing_up.jpg"));
    std::mt19937 random(5);
    std::normal_distribution<float> noise(0.0f, 6.0f);
    std::optional<Vec2> last;
    float steps = 0.0f, worst = 0.0f;
    constexpr int kFrames = 60;
    for (int i = 0; i < kFrames; ++i) {
        std::optional<HandLandmarks> hand = First(m_tracker->Track(Noisy(photo, random, noise)));
        ASSERT_TRUE(hand);
        Vec2 tip = hand->At(HandJoint::IndexTip).Xy();
        if (last) {
            float step = (tip - *last).Length();
            steps += step;
            worst = std::max(worst, step);
        }
        last = tip;
    }
    EXPECT_LT(steps / (kFrames - 1), 1.0f);
    EXPECT_LT(worst, 6.0f);
}

// The whole way from camera to pointer on a hand that doesn't move: the pointer mustn't
// either, from the first picture on.
TEST_F(MediaPipeHandTrackerTest, AStillHandLeavesThePointerStill) {
    CameraFrame photo = testing::ImageFile::Load(testing::ImageFile::TestData(L"pointing_up.jpg"));
    testing::FakePointer pointer;
    HandGestureController controller(*m_tracker, pointer, HandGestureSettings::kDefaultSpeed);
    for (int frame = 1; frame <= 45; ++frame) {
        EXPECT_EQ(controller.Process(photo, frame / 30.0).mode, GestureMode::Steering);
    }
    EXPECT_NEAR(pointer.position.x, 0.5f, 1e-4f);
    EXPECT_NEAR(pointer.position.y, 0.5f, 1e-4f);
}

TEST_F(MediaPipeHandTrackerTest, FindsNoHandInABlankPicture) {
    CameraFrame blank(320, 240, std::vector<uint8_t>(320 * 240 * 4, 128));
    EXPECT_TRUE(m_tracker->Track(blank).empty());
}

}  // namespace
}  // namespace gazeora::input
