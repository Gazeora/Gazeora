// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>
#include <xinput.h>

#include <cstddef>
#include <cstdint>

#include "gazeora/hook/input/stick_turn.h"

namespace gazeora::hook::input {

// Sony's controllers on USB, by their HID device path's product id.
enum class SonyPad { None, DualSense, DualShock4 };

// What `path` (a HID device path, any case) is: a Sony controller the hook knows, or None.
SonyPad SonyPadAt(const wchar_t* path);
// The same from a HID device's vendor and product ids.
SonyPad SonyPadWithIds(uint16_t vendor, uint16_t product);

// A Sony controller's USB input report (id 1): both sticks a byte per axis, 128 the middle,
// y down; then its triggers and buttons, laid out per controller.
class SonyUsbReport {
public:
    SonyUsbReport(SonyPad pad, uint8_t* data, size_t size)
        : m_pad(pad), m_data(data), m_size(size) {}

    // An input report of a controller the hook knows.
    bool Valid() const;
    // Whether the player is using it: a stick pushed, a trigger pulled or a button pressed.
    bool Active() const;
    // `push` added to the right stick, within its range.
    void AddToRightStick(const StickPush& push);

private:
    bool ButtonsPressed() const;
    bool TriggersPulled() const;

    SonyPad m_pad;
    uint8_t* m_data;
    size_t m_size;
};

// The same for an XInput controller's state.
class XInputPadState {
public:
    explicit XInputPadState(XINPUT_GAMEPAD& pad) : m_pad(pad) {}

    bool Active() const;
    void AddToRightStick(const StickPush& push);

private:
    XINPUT_GAMEPAD& m_pad;
};

}  // namespace gazeora::hook::input
