// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

namespace gazeora::app {

// The one line under the head tracking settings: whether head turns reach a game, and through
// which, or what they're waiting for.
class HeadTrackingStatusLine {
public:
    struct State {
        bool enabled = false;
        bool receiving = false;
        bool gameInFront = false;
        bool onController = false;
    };

    explicit HeadTrackingStatusLine(const State& state);

    const char* Text() const { return m_text; }
    ImU32 Color() const { return m_color; }

private:
    void DescribeOn(const State& state);

    const char* m_text;
    ImU32 m_color;
};

}  // namespace gazeora::app
