// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "head/head_look.h"
#include "head/mouse_look_driver.h"
#include "settings/controls_settings.h"

namespace gazeora::app {

// The head tracking switch and sensitivity, saved, and applied to the head look and its
// mouse-look driver.
class HeadTrackingControl {
public:
    // Applies the saved settings.
    HeadTrackingControl(HeadLook& look, MouseLookDriver& mouse, const ControlsSettings& store);

    bool Enabled() const { return m_look.Enabled(); }
    void SetEnabled(bool enabled);
    float CountsPerDegree() const { return m_mouse.CountsPerDegree(); }
    // Applies at once; Save keeps it, once the slider is let go.
    void SetCountsPerDegree(float counts) { m_mouse.SetCountsPerDegree(counts); }
    void SaveCountsPerDegree() const;

    bool Receiving() const { return m_look.Receiving(); }
    bool GameInFront() const { return m_look.GameInFront(); }
    // A few words on what head tracking does, for the stream overlay; empty while it's off.
    std::wstring OverlayStatus() const;

private:
    HeadLook& m_look;
    MouseLookDriver& m_mouse;
    const ControlsSettings& m_store;
};

}  // namespace gazeora::app
