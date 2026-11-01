// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

#include "gazeora/plugin_ui/models/ui_element.h"
#include "gazeora/plugin_ui/models/ui_tone.h"

namespace gazeora::ui {

// One <cell> of a table row: text, in a tone.
struct UiCell {
    std::string text;
    UiTone tone = UiTone::Normal;
};

// <table>: rows of cells, each column as wide as its widest cell.
struct UiTable : UiVisitable<UiTable> {
    std::vector<std::vector<UiCell>> rows;
    int columns = 0;  // the most cells in a row
};

}  // namespace gazeora::ui
