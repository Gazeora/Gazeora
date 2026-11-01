// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gamepad/gamepad_control.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

#include "support/gamepad_fakes.h"

namespace gazeora::app {
namespace {

constexpr auto kTimeout = std::chrono::seconds(5);

class GamepadControlTest : public ::testing::Test {
protected:
    bool WaitForConnected(const GamepadControl& control) {
        auto deadline = std::chrono::steady_clock::now() + kTimeout;
        while (!control.Connected() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return control.Connected();
    }

    input::testing::FakeGamepads m_gamepads;
    std::shared_ptr<input::testing::FakeGamepadDriverFactory> m_drivers =
        std::make_shared<input::testing::FakeGamepadDriverFactory>(m_gamepads);
    std::shared_ptr<input::testing::FakeGamepadSwitch> m_switch =
        std::make_shared<input::testing::FakeGamepadSwitch>();
    input::testing::RecordingGamepadListener m_listener;
    input::GamepadSession m_session{m_drivers, m_listener};
};

TEST_F(GamepadControlTest, ReadsControllersWhileThePluginsSwitchIsOn) {
    m_gamepads.Connect(L"DualSense Wireless Controller");
    GamepadControl control(m_session, m_switch);
    EXPECT_TRUE(control.Enabled());
    control.Refresh();
    EXPECT_TRUE(WaitForConnected(control));
}

TEST_F(GamepadControlTest, FollowsThePluginsSwitch) {
    GamepadControl control(m_session, m_switch);
    control.Refresh();
    EXPECT_TRUE(m_session.Running());
    m_switch->on = false;
    control.Refresh();
    EXPECT_FALSE(control.Enabled());
    EXPECT_FALSE(m_session.Running());
    m_switch->on = true;
    control.Refresh();
    EXPECT_TRUE(m_session.Running());
}

TEST_F(GamepadControlTest, ReadsNothingWithoutTheControllerPlugin) {
    input::GamepadSession session(nullptr, m_listener);
    GamepadControl control(session, nullptr);
    EXPECT_FALSE(control.Available());
    EXPECT_FALSE(control.Enabled());
    control.Refresh();
    EXPECT_FALSE(session.Running());
    EXPECT_FALSE(control.Connected());
}

TEST_F(GamepadControlTest, LeavesControllersToARunningGame) {
    GamepadControl control(m_session, m_switch);
    control.Refresh();
    control.SetGameRunning(true);
    EXPECT_FALSE(m_session.Running());
    EXPECT_TRUE(control.Paused());
    control.SetGameRunning(false);
    EXPECT_TRUE(m_session.Running());
    EXPECT_FALSE(control.Paused());
}

TEST_F(GamepadControlTest, TriesAgainAfterAnError) {
    m_drivers->failure = L"Game controllers can't be read: no HID.";
    GamepadControl control(m_session, m_switch);
    control.Refresh();
    auto deadline = std::chrono::steady_clock::now() + kTimeout;
    while (control.Snapshot().state != input::GamepadSessionState::Error &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_EQ(control.Snapshot().state, input::GamepadSessionState::Error);
    m_drivers->failure.clear();
    m_gamepads.Connect(L"Xbox Controller");
    control.Restart();
    EXPECT_TRUE(WaitForConnected(control));
}

TEST_F(GamepadControlTest, ReadsWithAPluginInstalledWhileRunning) {
    input::GamepadSession session(nullptr, m_listener);
    GamepadControl control(session, nullptr);
    control.Refresh();
    EXPECT_FALSE(control.Available());
    m_gamepads.Connect(L"Xbox Controller");
    control.SetDriver(m_drivers, m_switch);
    EXPECT_TRUE(control.Available());
    EXPECT_TRUE(WaitForConnected(control));
    control.SetDriver(nullptr, nullptr);  // uninstalled
    EXPECT_FALSE(control.Available());
    EXPECT_FALSE(session.Running());
}

TEST_F(GamepadControlTest, KeepsReadingWhenTheDriverStaysTheSame) {
    m_gamepads.Connect(L"Xbox Controller");
    GamepadControl control(m_session, m_switch);
    control.Refresh();
    ASSERT_TRUE(WaitForConnected(control));
    const uint64_t version = control.Version();
    control.SetDriver(m_drivers, m_switch);  // another plugin changed, not this one
    EXPECT_TRUE(control.Connected());
    EXPECT_EQ(control.Version(), version);
}

}  // namespace
}  // namespace gazeora::app
