// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_ui/models/ui_condition.h"

namespace gazeora::ui {

namespace {

constexpr char kNot = '!';
constexpr char kFalseNumber[] = "0";
constexpr char kFalseWord[] = "false";

}  // namespace

UiCondition::UiCondition(const std::string& text)
    : m_key(!text.empty() && text[0] == kNot ? text.substr(1) : text),
      m_negated(!text.empty() && text[0] == kNot) {}

bool UiCondition::Holds(const IUiValues& values) const {
    return m_key.empty() || IsTrue(values.Get(m_key)) != m_negated;
}

bool UiCondition::IsTrue(const std::string& value) {
    return !value.empty() && value != kFalseNumber && value != kFalseWord;
}

}  // namespace gazeora::ui
