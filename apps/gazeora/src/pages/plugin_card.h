// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

#include "plugins/plugin_manager_control.h"

namespace gazeora::app {

// One plugin as a tile in the Plugins page's grid: its category's icon, its name and version,
// its category, what it is, and its actions (install, update, uninstall, website) along the bottom.
class PluginCard {
public:
    struct Fonts {
        ImFont* bold;
        ImFont* caption;
        ImFont* heading;
    };

    PluginCard(PluginManagerControl& control, float scale) : m_control(control), m_scale(scale) {}

    // Draws the tile at `size`; returns the height its contents need, for the next frame's row.
    float Draw(const Fonts& fonts, const PluginEntry& entry, ImVec2 size);

private:
    struct Action {
        std::string label;
        bool primary;
        bool enabled;
        std::function<void()> run;
    };

    void BeginTile(ImVec2 size);
    void EndTile();
    std::vector<Action> ActionsFor(const PluginEntry& entry) const;
    void DrawHeading(const Fonts& fonts, const PluginEntry& entry);
    void DrawIcon(const Fonts& fonts, const char* icon);
    void DrawBadges(const Fonts& fonts, const PluginEntry& entry);
    void DrawBadge(const Fonts& fonts, const char* text, ImU32 background, ImU32 color);
    void DrawAbout(const PluginEntry& entry);
    void DrawNotes(const PluginEntry& entry);
    void DrawActions(const Fonts& fonts, const std::vector<Action>& actions);
    float S(float px) const { return px * m_scale; }

    PluginManagerControl& m_control;
    const float m_scale;
};

}  // namespace gazeora::app
