// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_ui/models/ui_element.h"
#include "gazeora/plugin_ui/models/ui_tone.h"

namespace gazeora::ui {

// <text>: wrapped text, written in the layout or the value under `bind`.
struct UiText : UiVisitable<UiText> {
    std::string text;
    std::string bind;  // shown instead of text when set
    UiTone tone = UiTone::Normal;
    std::string toneBind;  // a tone id under this key, instead of tone when set
};

}  // namespace gazeora::ui
