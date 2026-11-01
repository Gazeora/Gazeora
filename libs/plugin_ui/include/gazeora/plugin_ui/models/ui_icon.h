// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <optional>
#include <string>

namespace gazeora::ui {

// The icons a layout can put on a panel's title or a button: Gazeora's own, by name.
enum class UiIcon {
    None,
    Add,
    Camera,
    Check,
    Chip,
    Clock,
    Delete,
    Edit,
    Folder,
    Game,
    Globe,
    Hand,
    Info,
    Play,
    Plugin,
    Refresh,
    Search,
    Settings,
    Shop,
    Warning,
};

// Icons by their names in a layout ("game", "refresh", ...; "" for None).
class UiIcons final {
public:
    UiIcons() = delete;

    static std::optional<UiIcon> Parse(const std::string& name);
};

}  // namespace gazeora::ui
