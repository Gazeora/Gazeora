// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <optional>
#include <string>

namespace gazeora::ui {

// The color text is shown in, by what it means rather than the color itself.
enum class UiTone { Normal, Dim, Faint, Accent, Success, Warning, Danger };

// Tones by their ids in a layout: "normal", "dim", "faint", "accent", "success", "warning",
// "danger".
class UiTones final {
public:
    UiTones() = delete;

    static std::optional<UiTone> Parse(const std::string& id);
    static const char* IdOf(UiTone tone);
};

}  // namespace gazeora::ui
