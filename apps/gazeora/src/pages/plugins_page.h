// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include <imgui.h>

#include <string>

#include "pages/plugin_category_filter.h"
#include "pages/plugin_grid.h"
#include "plugins/plugin_manager_control.h"

namespace gazeora::app {

// The Plugins page: the installed plugins, and the ones that can be installed (those that come
// with Gazeora and the plugin list's), as grids of tiles, searchable, with install, update and
// uninstall, and installing a DLL from disk.
class PluginsPage {
public:
    struct Fonts {
        ImFont* title;
        ImFont* heading;
        ImFont* caption;
        ImFont* bold;
    };

    PluginsPage(PluginManagerControl& control, std::wstring folder, HWND owner, float scale)
        : m_control(control),
          m_grid(control, scale),
          m_filter(scale),
          m_folder(std::move(folder)),
          m_owner(owner),
          m_scale(scale) {}

    void Draw(const Fonts& fonts);

private:
    void DrawHeader(const Fonts& fonts);
    void DrawSearch(float width);
    void DrawToolbar(const Fonts& fonts, float width);
    void DrawStatus();
    // The entries in one section that match the search; how many did.
    int DrawSection(const Fonts& fonts, const char* title, bool installed, float width);
    void DrawSectionTitle(const Fonts& fonts, const char* title, int count);
    void DrawAvailableNote(int shown);
    bool InInstalledSection(const PluginEntry& entry) const;
    // Whether the search and the category filter let it through.
    bool Shown(const PluginEntry& entry) const;
    // Whether a search or a category narrows what's shown.
    bool Filtering() const;
    float S(float px) const { return px * m_scale; }

    PluginManagerControl& m_control;
    PluginGrid m_grid;
    PluginCategoryFilter m_filter;
    const std::wstring m_folder;
    const HWND m_owner;
    float m_scale;
    std::string m_query;
};

}  // namespace gazeora::app
