// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_ui/models/ui_element.h"

namespace gazeora::ui {

// <slider>: a number from min to max under `bind`, its label above it.
struct UiSlider : UiVisitable<UiSlider> {
    std::string bind;
    std::string label;
    std::string help;
    float min = 0.0f;
    float max = 1.0f;
    std::string format;  // printf-style, as "%.0f%%"
};

}  // namespace gazeora::ui
