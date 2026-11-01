// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora::ui {

// The values a UI panel shows and changes, by key, as text; and its buttons' actions. Called on
// the UI thread.
class IUiValues {
public:
    virtual ~IUiValues() = default;

    // "" if there's nothing under `key`.
    virtual std::string Get(const std::string& key) const = 0;
    virtual void Set(const std::string& key, const std::string& value) = 0;
    virtual void Act(const std::string& action) = 0;
};

}  // namespace gazeora::ui
