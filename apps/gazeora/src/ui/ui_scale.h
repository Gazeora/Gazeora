// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

namespace gazeora::app {

// How big the launcher draws on a monitor: its DPI scaling, enlarged for easy reading as far
// as the smallest window still fits the screen.
// Factory Method pattern: ForMonitor.
class UiScale {
public:
    static UiScale ForMonitor(HMONITOR monitor);

    // Multiplies every font and layout size.
    float Factor() const { return m_factor; }
    SIZE MinWindowSize() const;
    // The starting window, no bigger than the screen.
    SIZE DefaultWindowSize() const;
    const RECT& WorkArea() const { return m_workArea; }

private:
    UiScale(float factor, const RECT& workArea) : m_factor(factor), m_workArea(workArea) {}

    float m_factor;
    RECT m_workArea;
};

}  // namespace gazeora::app
