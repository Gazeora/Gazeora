// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/gazeora_ui_values.h"

namespace gazeora::app {

namespace {

constexpr char kTrue[] = "1";
constexpr char kFalse[] = "0";

}  // namespace

template <typename Function>
void GazeoraUiValues::Put(Table<Function>& table, const std::string& key, Function function) {
    for (auto& entry : table) {
        if (entry.first == key) {
            entry.second = std::move(function);
            return;
        }
    }
    table.emplace_back(key, std::move(function));
}

template <typename Function>
const Function* GazeoraUiValues::Find(const Table<Function>& table, const std::string& key) {
    for (const auto& entry : table) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

void GazeoraUiValues::AddValue(const std::string& key, Getter getter) {
    Put(m_values, key, std::move(getter));
}

void GazeoraUiValues::AddAction(const std::string& action, Command command) {
    Put(m_actions, action, std::move(command));
}

std::string GazeoraUiValues::Get(const std::string& key) const {
    const Getter* getter = Find(m_values, key);
    return getter ? (*getter)() : std::string();
}

void GazeoraUiValues::Act(const std::string& action) {
    if (const Command* command = Find(m_actions, action)) {
        (*command)();
    }
}

std::string GazeoraUiValues::Flag(bool value) { return value ? kTrue : kFalse; }

}  // namespace gazeora::app
