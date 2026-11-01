// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "gazeora/plugin_ui/interfaces/ui_values.h"

namespace gazeora::app {

// Gazeora's own values and actions that plugin panels bind to, under "gazeora." keys
// (docs/plugin-ui.md). Read-only: a panel can't set them. Command pattern: each action is a
// function, run when a panel's button asks for it. A handful of each, looked up every frame:
// kept side by side and searched in order, which beats a tree's scattered nodes at this size.
class GazeoraUiValues final : public ui::IUiValues {
public:
    using Getter = std::function<std::string()>;
    using Command = std::function<void()>;

    void AddValue(const std::string& key, Getter getter);
    void AddAction(const std::string& action, Command command);

    // "" for a key it doesn't have.
    std::string Get(const std::string& key) const override;
    // Ignored: Gazeora's values change only through Gazeora.
    void Set(const std::string&, const std::string&) override {}
    void Act(const std::string& action) override;

    static std::string Flag(bool value);

private:
    template <typename Function>
    using Table = std::vector<std::pair<std::string, Function>>;

    template <typename Function>
    static void Put(Table<Function>& table, const std::string& key, Function function);
    template <typename Function>
    static const Function* Find(const Table<Function>& table, const std::string& key);

    Table<Getter> m_values;
    Table<Command> m_actions;
};

}  // namespace gazeora::app
