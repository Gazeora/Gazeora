// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_ui/models/ui_element.h"

namespace gazeora::ui {

// <input>: one line of text under `bind`, its label above it. A game controller types into it
// with the on-screen keyboard, as into Gazeora's own text boxes.
struct UiInput : UiVisitable<UiInput> {
    std::string bind;
    std::string label;
    std::string help;
    std::string hint;  // shown while it's empty
};

}  // namespace gazeora::ui
