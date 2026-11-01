// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

#include "gazeora/plugin_ui/models/ui_element.h"

namespace gazeora::ui {

// One <option> of a choice: the value it sets, and what the list shows.
struct UiOption {
    std::string value;
    std::string label;
};

// <choice>: a drop-down list setting the value under `bind` to its option's value.
struct UiChoice : UiVisitable<UiChoice> {
    std::string bind;
    std::string label;
    std::string help;
    std::vector<UiOption> options;
};

}  // namespace gazeora::ui
