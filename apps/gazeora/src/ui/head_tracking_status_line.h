// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

namespace gazeora::app {

// The one line under the head tracking switch: whether head turns reach a game, or what
// they're waiting for.
class HeadTrackingStatusLine {
public:
    HeadTrackingStatusLine(bool enabled, bool receiving, bool gameInFront);

    const char* Text() const { return m_text; }
    ImU32 Color() const { return m_color; }

private:
    void DescribeOn(bool receiving, bool gameInFront);

    const char* m_text;
    ImU32 m_color;
};

}  // namespace gazeora::app
