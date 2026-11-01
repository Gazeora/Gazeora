// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/gamepad_ui_values.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

#include "support/gamepad_fakes.h"

namespace gazeora::app {
namespace {

constexpr auto kTimeout = std::chrono::seconds(5);

class GamepadUiValuesTest : public ::testing::Test {
protected:
    void SetUp() override { GamepadUiValues::AddTo(values, control); }

    bool WaitFor(const char* key, const std::string& value) {
        auto deadline = std::chrono::steady_clock::now() + kTimeout;
        while (values.Get(key) != value && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return values.Get(key) == value;
    }

    input::testing::FakeGamepads gamepads;
    std::shared_ptr<input::testing::FakeGamepadDriverFactory> drivers =
        std::make_shared<input::testing::FakeGamepadDriverFactory>(gamepads);
    std::shared_ptr<input::testing::FakeGamepadSwitch> enabled =
        std::make_shared<input::testing::FakeGamepadSwitch>();
    input::testing::RecordingGamepadListener listener;
    input::GamepadSession session{drivers, listener};
    GamepadControl control{session, enabled};
    GazeoraUiValues values;
};

TEST_F(GamepadUiValuesTest, SaysWhatIsConnected) {
    gamepads.Connect(L"DualSense Wireless Controller");
    control.Refresh();
    ASSERT_TRUE(WaitFor("gazeora.gamepad.tone", "success"));
    EXPECT_NE(values.Get("gazeora.gamepad.status").find("DualSense"), std::string::npos);
    EXPECT_EQ(values.Get("gazeora.gamepad.failed"), "0");
}

TEST_F(GamepadUiValuesTest, SaysOffWhileThePluginsSwitchIsOff) {
    enabled->on = false;
    control.Refresh();
    EXPECT_EQ(values.Get("gazeora.gamepad.status"), "Off");
    EXPECT_EQ(values.Get("gazeora.gamepad.tone"), "faint");
}

TEST_F(GamepadUiValuesTest, RetryReadsThemAgainAfterAnError) {
    drivers->failure = L"Game controllers can't be read: no HID.";
    control.Refresh();
    ASSERT_TRUE(WaitFor("gazeora.gamepad.failed", "1"));
    EXPECT_EQ(values.Get("gazeora.gamepad.status"), "Game controllers can't be read: no HID.");
    EXPECT_EQ(values.Get("gazeora.gamepad.tone"), "danger");
    drivers->failure.clear();
    values.Act("gazeora.gamepad.retry");
    EXPECT_TRUE(WaitFor("gazeora.gamepad.failed", "0"));
}

TEST(GazeoraUiValuesTest, IgnoresWhatItDoesntHave) {
    GazeoraUiValues values;
    EXPECT_EQ(values.Get("gazeora.nothing"), "");
    values.Act("gazeora.nothing");  // no throw
    values.Set("gazeora.game-running", "1");
    EXPECT_EQ(values.Get("gazeora.game-running"), "");
}

}  // namespace
}  // namespace gazeora::app
