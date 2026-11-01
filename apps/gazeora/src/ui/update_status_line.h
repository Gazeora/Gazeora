// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include <string>

#include "update/update_control.h"

namespace gazeora::app {

// The one line in the Updates card saying where the update check is.
class UpdateStatusLine {
public:
    explicit UpdateStatusLine(const UpdateControl& control);

    const std::string& Text() const { return m_text; }
    ImU32 Color() const { return m_color; }

private:
    void Describe(const UpdateControl& control);
    std::string UpdateLabel(const UpdateControl& control) const;

    std::string m_text;
    ImU32 m_color;
};

}  // namespace gazeora::app
