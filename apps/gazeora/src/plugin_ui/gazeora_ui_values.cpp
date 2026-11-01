// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/gazeora_ui_values.h"

namespace gazeora::app {

namespace {

constexpr char kTrue[] = "1";
constexpr char kFalse[] = "0";

}  // namespace

void GazeoraUiValues::AddValue(const std::string& key, Getter getter) {
    m_values[key] = std::move(getter);
}

void GazeoraUiValues::AddAction(const std::string& action, Command command) {
    m_actions[action] = std::move(command);
}

std::string GazeoraUiValues::Get(const std::string& key) const {
    auto found = m_values.find(key);
    return found == m_values.end() ? std::string() : found->second();
}

void GazeoraUiValues::Act(const std::string& action) {
    auto found = m_actions.find(action);
    if (found != m_actions.end()) {
        found->second();
    }
}

std::string GazeoraUiValues::Flag(bool value) { return value ? kTrue : kFalse; }

}  // namespace gazeora::app
