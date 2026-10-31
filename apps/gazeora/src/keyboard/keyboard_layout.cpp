// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "keyboard/keyboard_layout.h"

#include <cstring>

#include "keyboard/keys/character_key.h"
#include "keyboard/keys/done_key.h"
#include "keyboard/keys/erase_key.h"
#include "keyboard/keys/layer_key.h"
#include "keyboard/keys/shift_key.h"
#include "keyboard/keys/space_key.h"

namespace gazeora::app {

namespace {

constexpr char kToSymbols[] = "&123";
constexpr char kToLetters[] = "ABC";

}  // namespace

KeyboardLayout KeyboardLayout::Letters() {
    KeyboardLayout layout;
    layout.m_rows.push_back(Characters("1234567890", "!@#$%^&*()"));
    layout.m_rows.push_back(Characters("qwertyuiop", "QWERTYUIOP"));
    layout.m_rows.push_back(Characters("asdfghjkl-", "ASDFGHJKL_"));
    Row bottom;
    bottom.push_back(std::make_unique<ShiftKey>());
    for (auto& key : Characters("zxcvbnm.", "ZXCVBNM,")) {
        bottom.push_back(std::move(key));
    }
    layout.m_rows.push_back(std::move(bottom));
    layout.m_rows.push_back(Controls(kToSymbols));
    return layout;
}

KeyboardLayout KeyboardLayout::Symbols() {
    KeyboardLayout layout;
    layout.m_rows.push_back(Characters("1234567890", "1234567890"));
    layout.m_rows.push_back(Characters("!@#$%^&*()", "!@#$%^&*()"));
    layout.m_rows.push_back(Characters("-_=+[]{};:", "-_=+[]{};:"));
    layout.m_rows.push_back(Characters("'\",./\\?~|`", "'\",./\\?~|`"));
    layout.m_rows.push_back(Controls(kToLetters));
    return layout;
}

KeyboardLayout::Row KeyboardLayout::Characters(const char* plain, const char* shifted) {
    Row row;
    for (size_t i = 0; i < std::strlen(plain); ++i) {
        row.push_back(std::make_unique<CharacterKey>(plain[i], shifted[i]));
    }
    return row;
}

KeyboardLayout::Row KeyboardLayout::Controls(const char* layerLabel) {
    Row row;
    row.push_back(std::make_unique<LayerKey>(layerLabel));
    row.push_back(std::make_unique<SpaceKey>());
    row.push_back(std::make_unique<EraseKey>());
    row.push_back(std::make_unique<DoneKey>());
    return row;
}

}  // namespace gazeora::app
