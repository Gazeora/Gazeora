#include "gazeora/input/services/gamepad_session.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <thread>

#include "support/gamepad_fakes.h"

namespace gazeora::input {
namespace {

constexpr auto kTimeout = std::chrono::seconds(5);
// Long enough for the session to have polled a good many times.
constexpr auto kSettle = std::chrono::milliseconds(100);

class GamepadSessionTest : public ::testing::Test {
protected:
    // Waits until `done` holds or the timeout passes.
    bool WaitFor(const std::function<bool()>& done) {
        auto deadline = std::chrono::steady_clock::now() + kTimeout;
        while (!done() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return done();
    }

    static GamepadState Pressed(GamepadButton button) {
        GamepadState state;
        state.SetButton(button, true);
        return state;
    }

    testing::FakeGamepads m_gamepads;
    testing::FakeGamepadDriverFactory m_drivers{m_gamepads};
    testing::RecordingGamepadListener m_listener;
    GamepadSession m_session{m_drivers, m_listener};
};

TEST_F(GamepadSessionTest, ListsTheConnectedControllers) {
    m_gamepads.Connect(L"DualSense Wireless Controller");
    m_session.Start();
    EXPECT_TRUE(WaitFor([&] { return !m_session.Snapshot().connected.empty(); }));
    GamepadSnapshot snapshot = m_session.Snapshot();
    EXPECT_EQ(snapshot.state, GamepadSessionState::Running);
    EXPECT_EQ(snapshot.connected.front().name, L"DualSense Wireless Controller");
    m_session.Stop();
    EXPECT_EQ(m_session.Snapshot().state, GamepadSessionState::Off);
}

TEST_F(GamepadSessionTest, TellsTheListenerOfEachChange) {
    m_session.Start();
    m_gamepads.Set(Pressed(GamepadButton::South));
    ASSERT_TRUE(WaitFor([&] { return m_listener.States().size() == 1; }));
    m_gamepads.Set(GamepadState());
    ASSERT_TRUE(WaitFor([&] { return m_listener.States().size() == 2; }));
    m_session.Stop();

    std::vector<GamepadState> states = m_listener.States();
    ASSERT_EQ(states.size(), 2u);
    EXPECT_TRUE(states[0].IsDown(GamepadButton::South));
    EXPECT_TRUE(states[1].IsNeutral());
}

TEST_F(GamepadSessionTest, StaysQuietWhileNothingChanges) {
    GamepadState drifting;
    drifting.SetAxis(GamepadAxis::LeftX, -0.1f);
    m_gamepads.Set(drifting);
    m_session.Start();
    std::this_thread::sleep_for(kSettle);
    m_session.Stop();
    EXPECT_TRUE(m_listener.States().empty());
}

TEST_F(GamepadSessionTest, LetsGoOfHeldButtonsWhenStopped) {
    m_gamepads.Set(Pressed(GamepadButton::DpadDown));
    m_session.Start();
    ASSERT_TRUE(WaitFor([&] { return !m_listener.States().empty(); }));
    m_session.Stop();
    EXPECT_TRUE(m_listener.States().back().IsNeutral());
}

TEST_F(GamepadSessionTest, SaysWhyControllersCantBeRead) {
    m_drivers.failure = L"Game controllers can't be read: no HID.";
    m_session.Start();
    EXPECT_TRUE(WaitFor([&] { return m_session.Snapshot().state == GamepadSessionState::Error; }));
    EXPECT_EQ(m_session.Snapshot().message, L"Game controllers can't be read: no HID.");
}

}  // namespace
}  // namespace gazeora::input
