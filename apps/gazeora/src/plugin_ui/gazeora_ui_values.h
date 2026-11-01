// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <functional>
#include <map>
#include <string>

#include "gazeora/plugin_ui/interfaces/ui_values.h"

namespace gazeora::app {

// Gazeora's own values and actions that plugin panels bind to, under "gazeora." keys
// (docs/plugin-ui.md). Read-only: a panel can't set them. Command pattern: each action is a
// function, run when a panel's button asks for it.
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
    std::map<std::string, Getter> m_values;
    std::map<std::string, Command> m_actions;
};

}  // namespace gazeora::app
