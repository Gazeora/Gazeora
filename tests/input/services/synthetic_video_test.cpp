// The whole way from camera to pointer, with the real models, on synthetic video: photos of
// hands moved, turned and tipped picture by picture.
#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>

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

constexpr double kFrame = 1.0 / 30;
constexpr int kSecond = 30;
const Vec2 kMiddle{320.0f, 260.0f};

class SyntheticVideoTest : public ::testing::Test {
protected:
    static testing::SyntheticVideo Video(const wchar_t* photo) {
        return testing::SyntheticVideo(
            testing::ImageFile::Load(testing::ImageFile::TestData(photo)));
    }

    // Plays `frames` pictures, the i-th made by `picture(t)` with t from 0 to 1; returns the
    // modes seen.
    std::vector<GestureMode> Play(int frames, const std::function<CameraFrame(float)>& picture) {
        std::vector<GestureMode> modes;
        for (int i = 0; i < frames; ++i) {
            m_now += kFrame;
            float t = frames > 1 ? float(i) / (frames - 1) : 1.0f;
            TrackingSnapshot snapshot = m_controller.Process(picture(t), m_now);
            EXPECT_EQ(snapshot.state, TrackingState::Tracking) << "picture " << i;
            modes.push_back(snapshot.mode);
        }
        return modes;
    }

    static bool All(const std::vector<GestureMode>& modes, GestureMode mode) {
        return std::all_of(modes.begin(), modes.end(), [&](GestureMode m) { return m == mode; });
    }

    OnnxRuntimeEngine m_engine{RuntimeFiles::OnnxRuntimeDll()};
    MediaPipeHandTrackerFactory m_factory{m_engine, RuntimeFiles::PalmModel(),
                                          RuntimeFiles::LandmarkModel()};
    std::unique_ptr<IHandTracker> m_tracker = m_factory.Create();
    testing::FakePointer m_pointer;
    HandGestureController m_controller{*m_tracker, m_pointer, HandGestureSettings::kDefaultSpeed};
    double m_now = 0.0;
};

TEST_F(SyntheticVideoTest, MovingTheHandUpMovesThePointerUp) {
    testing::SyntheticVideo video = Video(L"pointing_up.jpg");
    Play(kSecond / 2, [&](float) { return video.Frame(kMiddle); });
    auto modes =
        Play(kSecond / 3, [&](float t) { return video.Frame(kMiddle + Vec2{0, -80 * t}); });
    Play(kSecond / 2, [&](float) { return video.Frame(kMiddle + Vec2{0, -80}); });
    EXPECT_TRUE(All(modes, GestureMode::Steering));
    EXPECT_LT(m_pointer.position.y, 0.3f);
    EXPECT_NEAR(m_pointer.position.x, 0.5f, 0.03f);
}

TEST_F(SyntheticVideoTest, MovingTheHandToTheUsersRightMovesThePointerRight) {
    testing::SyntheticVideo video = Video(L"pointing_up.jpg");
    Play(kSecond / 2, [&](float) { return video.Frame(kMiddle); });
    // The camera faces the user: their right is its left.
    Play(kSecond / 3, [&](float t) { return video.Frame(kMiddle + Vec2{-80 * t, 0}); });
    Play(kSecond / 2, [&](float) { return video.Frame(kMiddle + Vec2{-80, 0}); });
    EXPECT_GT(m_pointer.position.x, 0.7f);
    EXPECT_NEAR(m_pointer.position.y, 0.5f, 0.05f);
}

TEST_F(SyntheticVideoTest, TurningTheHandToTheUsersRightMovesThePointerRight) {
    testing::SyntheticVideo video = Video(L"pointing_up.jpg");
    Play(kSecond / 2, [&](float) { return video.Frame(kMiddle); });
    // The camera faces the user: their right is its left, an anticlockwise turn in the picture.
    Play(kSecond, [&](float t) { return video.Frame(kMiddle, -30 * t); });
    Play(kSecond / 2, [&](float) { return video.Frame(kMiddle, -30); });
    EXPECT_GT(m_pointer.position.x, 0.65f);
}

TEST_F(SyntheticVideoTest, TippingTheFingerTowardTheCameraMovesDownWithoutStopping) {
    testing::SyntheticVideo video = Video(L"pointing_up.jpg");
    Play(kSecond / 2, [&](float) { return video.Frame(kMiddle); });
    auto modes = Play(kSecond, [&](float t) { return video.Frame(kMiddle, 0, 50 * t); });
    EXPECT_TRUE(All(modes, GestureMode::Steering));
    EXPECT_GT(m_pointer.position.y, 0.52f);
}

TEST_F(SyntheticVideoTest, PointingDownStillMoves) {
    testing::SyntheticVideo video = Video(L"pointing_up.jpg");
    Play(kSecond / 2, [&](float) { return video.Frame(kMiddle, 180); });
    auto modes = Play(kSecond, [&](float t) { return video.Frame(kMiddle, 180 - 30 * t); });
    EXPECT_TRUE(All(modes, GestureMode::Steering));
    // Pointing down, the same turn swings the tip the other way.
    EXPECT_LT(m_pointer.position.x, 0.37f);
}

TEST_F(SyntheticVideoTest, PointingSidewaysTurnedAwaySteers) {
    testing::SyntheticVideo video = Video(L"pointing_up_rotated.jpg");
    auto modes = Play(kSecond, [&](float t) { return video.Frame(kMiddle, 0, -50 * t); });
    EXPECT_TRUE(All(modes, GestureMode::Steering));
}

TEST_F(SyntheticVideoTest, AFistWhileMovingKeepsMovingAndNeverClicks) {
    testing::SyntheticVideo pointing = Video(L"pointing_up.jpg");
    testing::SyntheticVideo fist = Video(L"fist.jpg");
    auto modes = Play(kSecond / 2, [&](float) { return pointing.Frame(kMiddle); });
    auto fisted = Play(kSecond / 2, [&](float) { return fist.Frame(kMiddle); });
    Play(kSecond / 2, [&](float) { return pointing.Frame(kMiddle); });
    EXPECT_TRUE(All(fisted, GestureMode::Steering));
    EXPECT_EQ(m_pointer.clicks, 0);
}

// The right hand on the camera's left, the left hand (a mirrored photo) on its right.
TEST_F(SyntheticVideoTest, TheLeftIndexUpThenTheRightIndexFoldingAndOpeningClicksOnce) {
    testing::SyntheticVideo pointing = Video(L"pointing_up.jpg");
    testing::SyntheticVideo fist = Video(L"fist.jpg");
    CameraFrame bothUp =
        testing::SyntheticVideo::SideBySide(pointing.Frame(kMiddle), pointing.Frame(kMiddle));
    CameraFrame rightFolded =
        testing::SyntheticVideo::SideBySide(fist.Frame(kMiddle), pointing.Frame(kMiddle));
    auto holding = Play(kSecond / 2, [&](float) { return bothUp; });
    Play(kSecond / 4, [&](float) { return rightFolded; });
    Play(kSecond / 4, [&](float) { return bothUp; });
    EXPECT_EQ(holding.back(), GestureMode::Holding);
    EXPECT_EQ(m_pointer.clicks, 1);
}

// Only the left hand in view: it never moves the pointer.
TEST_F(SyntheticVideoTest, TheLeftHandAloneLeavesThePointer) {
    testing::SyntheticVideo pointing = Video(L"pointing_up.jpg");
    CameraFrame blank = testing::SyntheticVideo::SideBySide(
        CameraFrame(testing::SyntheticVideo::kWidth, testing::SyntheticVideo::kHeight,
                    std::vector<uint8_t>(size_t(testing::SyntheticVideo::kWidth) *
                                             testing::SyntheticVideo::kHeight * 4,
                                         128)),
        pointing.Frame(kMiddle));
    for (int i = 0; i < kSecond; ++i) {
        m_now += kFrame;
        m_controller.Process(blank, m_now);
    }
    EXPECT_TRUE(m_pointer.moves.empty());
}

TEST_F(SyntheticVideoTest, AStillHandLeavesThePointerStill) {
    testing::SyntheticVideo video = Video(L"pointing_up.jpg");
    Play(kSecond * 2, [&](float) { return video.Frame(kMiddle); });
    EXPECT_NEAR(m_pointer.position.x, 0.5f, 1e-4f);
    EXPECT_NEAR(m_pointer.position.y, 0.5f, 1e-4f);
}

}  // namespace
}  // namespace gazeora::input
