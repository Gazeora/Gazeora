// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <stdexcept>
#include <string>

namespace gazeora::ui {

// A plugin's UI layout isn't valid XML, or not one Gazeora can draw. The message says what and
// on which line, for the plugin's author.
class UiLayoutError : public std::runtime_error {
public:
    explicit UiLayoutError(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace gazeora::ui
