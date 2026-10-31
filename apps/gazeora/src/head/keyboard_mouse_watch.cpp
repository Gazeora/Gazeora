// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/keyboard_mouse_watch.h"

namespace gazeora::app {

namespace {

// Generic desktop controls (HID usage tables).
constexpr USHORT kUsagePageGeneric = 0x01;
constexpr USHORT kUsageMouse = 0x02;
constexpr USHORT kUsageKeyboard = 0x06;

}  // namespace

void KeyboardMouseWatch::Register(HWND hwnd) {
    RAWINPUTDEVICE devices[] = {
        {kUsagePageGeneric, kUsageMouse, RIDEV_INPUTSINK, hwnd},
        {kUsagePageGeneric, kUsageKeyboard, RIDEV_INPUTSINK, hwnd},
    };
    RegisterRawInputDevices(devices, ARRAYSIZE(devices), sizeof(RAWINPUTDEVICE));
}

void KeyboardMouseWatch::OnRawInput(HRAWINPUT input) {
    RAWINPUTHEADER header;
    UINT size = sizeof(header);
    if (GetRawInputData(input, RID_HEADER, &header, &size, sizeof(RAWINPUTHEADER)) == UINT(-1) ||
        !header.hDevice) {
        return;  // injected, as head turns are
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    m_lastUsedQpc = now.QuadPart;
}

}  // namespace gazeora::app
