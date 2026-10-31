// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/input/pad_reports.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace gazeora::hook::input {
namespace {

using Report = std::array<uint8_t, 64>;

// A report with the controller left alone: sticks in the middle, the d-pad released.
Report Resting(SonyPad pad) {
    Report report = {};
    report[0] = 1;
    report[1] = report[2] = report[3] = report[4] = 128;
    report[pad == SonyPad::DualSense ? 8 : 5] = 8;
    return report;
}

TEST(SonyPadTest, KnowsTheControllersByPathOrIds) {
    EXPECT_EQ(SonyPadAt(L"\\\\?\\HID#VID_054C&PID_0CE6&MI_03#8&dfd99b&0&0000#{4d1e55b2}"),
              SonyPad::DualSense);
    EXPECT_EQ(SonyPadAt(L"\\\\?\\hid#vid_054c&pid_09cc#..."), SonyPad::DualShock4);
    EXPECT_EQ(SonyPadAt(L"\\\\?\\HID#VID_046D&PID_C52B#..."), SonyPad::None);
    EXPECT_EQ(SonyPadAt(nullptr), SonyPad::None);
    EXPECT_EQ(SonyPadWithIds(0x054C, 0x0DF2), SonyPad::DualSense);
    EXPECT_EQ(SonyPadWithIds(0x054C, 0x05C4), SonyPad::DualShock4);
    EXPECT_EQ(SonyPadWithIds(0x045E, 0x0CE6), SonyPad::None);
}

TEST(SonyUsbReportTest, OnlyInputReportsOfKnownControllers) {
    Report report = Resting(SonyPad::DualSense);
    EXPECT_TRUE(SonyUsbReport(SonyPad::DualSense, report.data(), report.size()).Valid());
    EXPECT_FALSE(SonyUsbReport(SonyPad::None, report.data(), report.size()).Valid());
    EXPECT_FALSE(SonyUsbReport(SonyPad::DualSense, report.data(), 5).Valid());
    report[0] = 0x31;  // Bluetooth's
    EXPECT_FALSE(SonyUsbReport(SonyPad::DualSense, report.data(), report.size()).Valid());
}

TEST(SonyUsbReportTest, ActiveWhenTheStickTriggersOrButtonsAreUsed) {
    for (SonyPad pad : {SonyPad::DualSense, SonyPad::DualShock4}) {
        Report report = Resting(pad);
        report[7] = 0xFC;  // the DualShock 4's counter, the DualSense's sequence
        EXPECT_FALSE(SonyUsbReport(pad, report.data(), report.size()).Active());
        report[1] = 128 + 20;  // a little drift
        EXPECT_FALSE(SonyUsbReport(pad, report.data(), report.size()).Active());
        report[3] = 20;  // the right stick pushed left
        EXPECT_TRUE(SonyUsbReport(pad, report.data(), report.size()).Active());

        report = Resting(pad);
        report[pad == SonyPad::DualSense ? 5 : 8] = 200;  // the left trigger
        EXPECT_TRUE(SonyUsbReport(pad, report.data(), report.size()).Active());

        report = Resting(pad);
        report[pad == SonyPad::DualSense ? 8 : 5] = 0x28;  // cross, d-pad released
        EXPECT_TRUE(SonyUsbReport(pad, report.data(), report.size()).Active());
    }
}

TEST(SonyUsbReportTest, AddsThePushToTheRightStick) {
    Report report = Resting(SonyPad::DualSense);
    SonyUsbReport(SonyPad::DualSense, report.data(), report.size())
        .AddToRightStick({-0.5, 0.5});  // left and up
    EXPECT_EQ(report[3], 65);           // 128 - 63.5
    EXPECT_EQ(report[4], 65);           // y runs down
    EXPECT_EQ(report[1], 128);          // the left stick left alone
    SonyUsbReport(SonyPad::DualSense, report.data(), report.size()).AddToRightStick({-1.0, 1.0});
    EXPECT_EQ(report[3], 0);  // within its range
    EXPECT_EQ(report[4], 0);
}

TEST(XInputPadStateTest, ActiveAndPushedLikeTheSonyOnes) {
    XINPUT_GAMEPAD gamepad = {};
    gamepad.sThumbLX = 3000;  // drift
    EXPECT_FALSE(XInputPadState(gamepad).Active());
    gamepad.wButtons = XINPUT_GAMEPAD_A;
    EXPECT_TRUE(XInputPadState(gamepad).Active());
    gamepad = {};
    XInputPadState(gamepad).AddToRightStick({0.5, 1.0});
    EXPECT_EQ(gamepad.sThumbRX, 16384);
    EXPECT_EQ(gamepad.sThumbRY, 32767);
}

}  // namespace
}  // namespace gazeora::hook::input
