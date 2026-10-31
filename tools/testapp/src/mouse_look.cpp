// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "testapp/mouse_look.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace testapp {

namespace {

constexpr float kRadiansPerDegree = 3.14159265f / 180.0f;
constexpr float kFullTurn = 360.0f;
constexpr float kHalfTurn = 180.0f;
constexpr DWORD kTitleEveryMs = 100;
// Generic desktop controls, mouse (HID usage tables).
constexpr USHORT kUsagePageGeneric = 0x01;
constexpr USHORT kUsageMouse = 0x02;

}  // namespace

void MouseLook::Attach(HWND hwnd) {
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    RAWINPUTDEVICE mouse = {kUsagePageGeneric, kUsageMouse, 0, hwnd};
    RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
}

MouseLook* MouseLook::Of(HWND hwnd) {
    return reinterpret_cast<MouseLook*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void MouseLook::OnRawInput(HRAWINPUT input) {
    RAWINPUT raw;
    UINT size = sizeof(raw);
    if (GetRawInputData(input, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == UINT(-1) ||
        raw.header.dwType != RIM_TYPEMOUSE || (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0) {
        return;
    }
    Turn(raw.data.mouse.lLastX, raw.data.mouse.lLastY);
}

void MouseLook::Turn(LONG dx, LONG dy) {
    // Right and down are positive counts: a turn to the right, a look down.
    m_yawDegrees = std::fmod(m_yawDegrees - dx * kDegreesPerCount + kHalfTurn, kFullTurn);
    if (m_yawDegrees < 0.0f) {
        m_yawDegrees += kFullTurn;
    }
    m_yawDegrees -= kHalfTurn;
    m_pitchDegrees =
        std::clamp(m_pitchDegrees - dy * kDegreesPerCount, -kMaxPitchDegrees, kMaxPitchDegrees);
}

float MouseLook::YawRadians() const { return m_yawDegrees * kRadiansPerDegree; }

float MouseLook::PitchRadians() const { return m_pitchDegrees * kRadiansPerDegree; }

void MouseLook::ShowInTitle(HWND hwnd) {
    const DWORD now = GetTickCount();
    if (now - m_titleAtMs < kTitleEveryMs) {
        return;
    }
    m_titleAtMs = now;
    wchar_t title[128];
    GetWindowTextW(hwnd, title, 128);
    // Keep the API name, which comes first, up to the angles of the last time.
    wchar_t* angles = wcsstr(title, L" - yaw");
    if (angles) {
        *angles = L'\0';
    }
    const size_t used = wcslen(title);
    swprintf(title + used, 128 - used, L" - yaw %.1f, pitch %.1f", double(m_yawDegrees),
             double(m_pitchDegrees));
    SetWindowTextW(hwnd, title);
}

}  // namespace testapp
