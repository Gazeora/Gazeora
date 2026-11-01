// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <map>
#include <string>
#include <vector>

#include "gazeora/plugin_ui/interfaces/ui_values.h"

namespace gazeora::ui::testing {

// Values a test sets, which remembers what a panel set and which actions it asked for.
class FakeUiValues final : public IUiValues {
public:
    std::string Get(const std::string& key) const override {
        read.push_back(key);
        auto found = values.find(key);
        return found == values.end() ? std::string() : found->second;
    }
    void Set(const std::string& key, const std::string& value) override { values[key] = value; }
    void Act(const std::string& action) override { actions.push_back(action); }

    std::map<std::string, std::string> values;
    std::vector<std::string> actions;
    mutable std::vector<std::string> read;
};

}  // namespace gazeora::ui::testing
