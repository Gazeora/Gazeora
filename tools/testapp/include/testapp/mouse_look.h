// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

namespace testapp {

// The 3D scene's camera, turned by the mouse as mouse-look games turn theirs: raw input counts,
// unaccelerated, while the window is in front. A count is kDegreesPerCount, so Gazeora's head
// tracking at its default 20 counts per degree turns it as far as the head turns.
class MouseLook {
public:
    static constexpr float kDegreesPerCount = 0.05f;
    static constexpr float kMaxPitchDegrees = 89.0f;

    // Asks for `hwnd`'s raw mouse input and keeps this camera with the window.
    void Attach(HWND hwnd);
    // The camera of a window Attach was given; null for other windows.
    static MouseLook* Of(HWND hwnd);

    // A WM_INPUT message's mouse movement.
    void OnRawInput(HRAWINPUT input);

    // Positive yaw to the left, positive pitch up.
    float YawRadians() const;
    float PitchRadians() const;
    // The angles in the window's title, now and then, so a turn can be checked on the PC.
    void ShowInTitle(HWND hwnd);

private:
    void Turn(LONG dx, LONG dy);

    float m_yawDegrees = 0.0f;
    float m_pitchDegrees = 0.0f;
    DWORD m_titleAtMs = 0;
};

}  // namespace testapp
