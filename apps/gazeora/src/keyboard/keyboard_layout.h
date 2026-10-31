// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <vector>

#include "keyboard/keyboard_key.h"

namespace gazeora::app {

// Factory pattern: the on-screen keyboard's keys, row by row, each row ten keys wide.
class KeyboardLayout {
public:
    using Row = std::vector<std::unique_ptr<IKeyboardKey>>;

    // Letters and digits; Shift gives capitals and the digits' symbols.
    static KeyboardLayout Letters();
    // Punctuation and the characters paths and arguments need.
    static KeyboardLayout Symbols();

    const std::vector<Row>& Rows() const { return m_rows; }
    const IKeyboardKey& Key(int row, int column) const { return *m_rows[row][column]; }
    int RowCount() const { return int(m_rows.size()); }
    int ColumnCount(int row) const { return int(m_rows[row].size()); }

private:
    // A row of characters: `plain[i]`, or `shifted[i]` with Shift on.
    static Row Characters(const char* plain, const char* shifted);
    static Row Controls(const char* layerLabel);

    std::vector<Row> m_rows;
};

}  // namespace gazeora::app
