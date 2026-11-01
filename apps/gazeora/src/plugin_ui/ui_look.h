// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include "gazeora/plugin_ui/models/ui_icon.h"
#include "gazeora/plugin_ui/models/ui_tone.h"

namespace gazeora::app {

// A layout's tones and icons in Gazeora's theme.
class UiLook final {
public:
    UiLook() = delete;

    static ImU32 Color(ui::UiTone tone);
    // The icon's glyph; "" for None.
    static const char* Glyph(ui::UiIcon icon);
};

}  // namespace gazeora::app
