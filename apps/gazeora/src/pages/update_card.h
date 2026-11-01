// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include "update/update_control.h"

namespace gazeora::app {

// The settings page's Updates card: the check-at-start switch, where the check is, and the
// buttons for its next step.
class UpdateCard {
public:
    UpdateCard(UpdateControl& control, float scale);

    // While `gameRunning`, installing waits: it closes Gazeora, and the game's stream with it.
    void Draw(ImFont* heading, float width, bool gameRunning);

private:
    void DrawSwitch();
    void DrawStatus();
    void DrawActions(bool gameRunning);
    void DrawInstallButtons(bool gameRunning);
    // Opens the newer release's page, where its MSI can be downloaded by hand.
    void DrawReleasePageButton(const char* label);
    float S(float px) const { return px * m_scale; }

    UpdateControl& m_control;
    float m_scale;
};

}  // namespace gazeora::app
