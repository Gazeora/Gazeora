// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "ui/head_tracking_status_line.h"

#include "ui/theme.h"

namespace gazeora::app {

HeadTrackingStatusLine::HeadTrackingStatusLine(const State& state)
    : m_text("Off"), m_color(kTextFaint) {
    if (state.enabled) {
        DescribeOn(state);
    }
}

void HeadTrackingStatusLine::DescribeOn(const State& state) {
    if (!state.receiving) {
        m_text = "Waiting for the phone: play in the Gazeora app, in a VR viewer";
        m_color = kTextDim;
    } else if (!state.gameInFront) {
        m_text =
            "The phone is tracking your head. Waiting for a game launched from here to "
            "be in front";
        m_color = kWarning;
    } else if (state.onController) {
        m_text = ICON_CHECK "  Your head is turning the game's camera with the controller";
        m_color = kSuccess;
    } else {
        m_text = ICON_CHECK "  Your head is turning the game's camera with the mouse";
        m_color = kSuccess;
    }
}

}  // namespace gazeora::app
