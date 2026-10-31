// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gamepad/gamepad_control.h"

#include <windows.h>

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

#include "support/gamepad_fakes.h"

namespace gazeora::app {
namespace {

constexpr auto kTimeout = std::chrono::seconds(5);

// controls.ini in the temp folder, removed afterwards.
std::wstring TempSettingsPath() {
    wchar_t folder[MAX_PATH];
    GetTempPathW(MAX_PATH, folder);
    return std::wstring(folder) + L"gazeora_gamepad_control_test_" +
           std::to_wstring(GetCurrentProcessId()) + L".ini";
}

class GamepadControlTest : public ::testing::Test {
protected:
    void TearDown() override { DeleteFileW(m_path.c_str()); }

    bool WaitForConnected(const GamepadControl& control) {
        auto deadline = std::chrono::steady_clock::now() + kTimeout;
        while (!control.Connected() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return control.Connected();
    }

    std::wstring m_path = TempSettingsPath();
    ControlsSettings m_store{m_path};
    input::testing::FakeGamepads m_gamepads;
    input::testing::FakeGamepadDriverFactory m_drivers{m_gamepads};
    input::testing::RecordingGamepadListener m_listener;
    input::GamepadSession m_session{m_drivers, m_listener};
};

TEST_F(GamepadControlTest, ReadsControllersUnlessSwitchedOff) {
    m_gamepads.Connect(L"DualSense Wireless Controller");
    GamepadControl control(m_session, m_store);
    EXPECT_TRUE(control.Enabled());
    control.Resume();
    EXPECT_TRUE(WaitForConnected(control));
}

TEST_F(GamepadControlTest, LeavesControllersToARunningGame) {
    GamepadControl control(m_session, m_store);
    control.Resume();
    control.SetGameRunning(true);
    EXPECT_FALSE(m_session.Running());
    EXPECT_TRUE(control.Paused());
    control.SetGameRunning(false);
    EXPECT_TRUE(m_session.Running());
    EXPECT_FALSE(control.Paused());
}

TEST_F(GamepadControlTest, RemembersTheSwitch) {
    GamepadControl control(m_session, m_store);
    control.Resume();
    control.SetEnabled(false);
    EXPECT_FALSE(m_session.Running());

    ControlsSettings reread(m_path);
    GamepadControl reopened(m_session, reread);
    EXPECT_FALSE(reopened.Enabled());
    reopened.Resume();
    EXPECT_FALSE(m_session.Running());
}

}  // namespace
}  // namespace gazeora::app
