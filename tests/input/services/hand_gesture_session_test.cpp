#include "gazeora/input/services/hand_gesture_session.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <thread>

#include "support/fakes.h"

namespace gazeora::input {
namespace {

constexpr auto kTimeout = std::chrono::seconds(5);

class HandGestureSessionTest : public ::testing::Test {
protected:
    // Polls the session until `done` holds or the timeout passes.
    TrackingSnapshot WaitFor(const std::function<bool(const TrackingSnapshot&)>& done) {
        auto deadline = std::chrono::steady_clock::now() + kTimeout;
        TrackingSnapshot snapshot = m_session.Snapshot();
        while (!done(snapshot) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            snapshot = m_session.Snapshot();
        }
        return snapshot;
    }

    testing::FakeCameraCatalog m_cameras;
    testing::SteadyHandTrackerFactory m_trackers;
    testing::FakePointer m_pointer;  // written by the session's thread: read after Stop
    testing::ManualClock m_clock;
    HandGestureSession m_session{m_cameras, m_trackers, m_pointer, m_clock};
};

TEST_F(HandGestureSessionTest, TracksUntilStopped) {
    m_session.Start(HandGestureSettings());
    TrackingSnapshot snapshot =
        WaitFor([](const TrackingSnapshot& s) { return s.state == TrackingState::Tracking; });
    EXPECT_EQ(snapshot.state, TrackingState::Tracking);
    m_session.Stop();
    EXPECT_EQ(m_session.Snapshot().state, TrackingState::Off);
    EXPECT_FALSE(m_pointer.moves.empty());
}

TEST_F(HandGestureSessionTest, SaysWhyTheCameraFailed) {
    m_cameras.failure = L"No camera is connected.";
    m_session.Start(HandGestureSettings());
    TrackingSnapshot snapshot =
        WaitFor([](const TrackingSnapshot& s) { return s.state == TrackingState::Error; });
    EXPECT_EQ(snapshot.state, TrackingState::Error);
    EXPECT_EQ(snapshot.message, L"No camera is connected.");
}

}  // namespace
}  // namespace gazeora::input
