// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include "gazeora/plugin_ui/interfaces/ui_values.h"
#include "plugin_ui/ui_input_buffers.h"

namespace gazeora::app {

// What drawing one plugin panel needs: the values it binds to, and Gazeora's look.
struct UiDrawContext {
    ui::IUiValues& values;
    UiInputBuffers& inputs;
    ImFont* caption;  // field labels
    float scale;

    float S(float px) const { return px * scale; }
};

}  // namespace gazeora::app
