// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/input/pad_reports.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <string>

namespace gazeora::hook::input {

namespace {

struct KnownPad {
    const wchar_t* ids;  // as in a HID device path, lower case
    uint16_t product;
    SonyPad pad;
};

constexpr uint16_t kSonyVendor = 0x054C;
constexpr KnownPad kSonyPads[] = {
    {L"vid_054c&pid_0ce6", 0x0CE6, SonyPad::DualSense},
    {L"vid_054c&pid_0df2", 0x0DF2, SonyPad::DualSense},  // DualSense Edge
    {L"vid_054c&pid_05c4", 0x05C4, SonyPad::DualShock4},
    {L"vid_054c&pid_09cc", 0x09CC, SonyPad::DualShock4},
};

constexpr uint8_t kUsbInputReport = 1;
constexpr size_t kLeftX = 1, kLeftY = 2, kRightX = 3, kRightY = 4;
constexpr int kStickMiddle = 128;
constexpr int kStickReach = 127;
// A stick this far from the middle is being pushed, not resting or drifting.
constexpr int kStickPushed = 32;
constexpr uint8_t kTriggerPulled = 30;
constexpr uint8_t kDpadMask = 0x0F;
constexpr uint8_t kDpadReleased = 8;
constexpr uint8_t kFaceButtonsMask = 0xF0;

// DualSense: triggers 5 and 6, the d-pad and face buttons in 8, the rest in 9, and PS, the
// touch pad and mute in 10's low bits.
constexpr size_t kDualSenseTriggers = 5, kDualSenseButtons = 8;
constexpr uint8_t kDualSenseSystemMask = 0x07;
// DualShock 4: the d-pad and face buttons in 5, the rest in 6, PS and the touch pad in 7's low
// bits (a counter above them), then the triggers in 8 and 9.
constexpr size_t kDualShockButtons = 5, kDualShockTriggers = 8;
constexpr uint8_t kDualShockSystemMask = 0x03;
constexpr size_t kReportSize = 11;

constexpr SHORT kXInputPushed = 8000;
constexpr double kXInputReach = 32767.0;

uint8_t Moved(uint8_t value, double push) {
    return uint8_t(std::clamp(int(std::lround(value + push * kStickReach)), 0, 255));
}

SHORT Moved(SHORT value, double push) {
    return SHORT(std::clamp(long(std::lround(value + push * kXInputReach)), -32768L, 32767L));
}

}  // namespace

SonyPad SonyPadAt(const wchar_t* path) {
    if (!path) {
        return SonyPad::None;
    }
    std::wstring lower(path);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](wchar_t c) { return wchar_t(std::towlower(c)); });
    for (const KnownPad& known : kSonyPads) {
        if (lower.find(known.ids) != std::wstring::npos) {
            return known.pad;
        }
    }
    return SonyPad::None;
}

SonyPad SonyPadWithIds(uint16_t vendor, uint16_t product) {
    if (vendor != kSonyVendor) {
        return SonyPad::None;
    }
    for (const KnownPad& known : kSonyPads) {
        if (known.product == product) {
            return known.pad;
        }
    }
    return SonyPad::None;
}

bool SonyUsbReport::Valid() const {
    return m_pad != SonyPad::None && m_data && m_size >= kReportSize &&
           m_data[0] == kUsbInputReport;
}

bool SonyUsbReport::Active() const {
    for (size_t axis : {kLeftX, kLeftY, kRightX, kRightY}) {
        if (std::abs(int(m_data[axis]) - kStickMiddle) > kStickPushed) {
            return true;
        }
    }
    return ButtonsPressed() || TriggersPulled();
}

bool SonyUsbReport::ButtonsPressed() const {
    const bool dualSense = m_pad == SonyPad::DualSense;
    const size_t at = dualSense ? kDualSenseButtons : kDualShockButtons;
    const uint8_t system = dualSense ? kDualSenseSystemMask : kDualShockSystemMask;
    return (m_data[at] & kDpadMask) != kDpadReleased || (m_data[at] & kFaceButtonsMask) != 0 ||
           m_data[at + 1] != 0 || (m_data[at + 2] & system) != 0;
}

bool SonyUsbReport::TriggersPulled() const {
    const size_t at = m_pad == SonyPad::DualSense ? kDualSenseTriggers : kDualShockTriggers;
    return m_data[at] > kTriggerPulled || m_data[at + 1] > kTriggerPulled;
}

void SonyUsbReport::AddToRightStick(const StickPush& push) {
    m_data[kRightX] = Moved(m_data[kRightX], push.x);
    m_data[kRightY] = Moved(m_data[kRightY], -push.y);  // the report's y runs down
}

bool XInputPadState::Active() const {
    for (SHORT axis : {m_pad.sThumbLX, m_pad.sThumbLY, m_pad.sThumbRX, m_pad.sThumbRY}) {
        if (std::abs(int(axis)) > kXInputPushed) {
            return true;
        }
    }
    return m_pad.wButtons != 0 || m_pad.bLeftTrigger > kTriggerPulled ||
           m_pad.bRightTrigger > kTriggerPulled;
}

void XInputPadState::AddToRightStick(const StickPush& push) {
    m_pad.sThumbRX = Moved(m_pad.sThumbRX, push.x);
    m_pad.sThumbRY = Moved(m_pad.sThumbRY, push.y);
}

}  // namespace gazeora::hook::input
