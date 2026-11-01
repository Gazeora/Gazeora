// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <optional>
#include <string>

namespace gazeora::ui {

// The pages a plugin's panel can go on: the sidebar's, but not a game's own page.
enum class UiPage { Home, Library, Controls, Plugins, Settings };

// Pages by their ids in a layout: "home", "library", "controls", "plugins", "settings".
class UiPages final {
public:
    UiPages() = delete;

    // Nothing for an id that isn't one of them.
    static std::optional<UiPage> Parse(const std::string& id);
};

}  // namespace gazeora::ui
