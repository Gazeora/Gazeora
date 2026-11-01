// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <map>
#include <string>
#include <vector>

#include "pages/plugin_card.h"

namespace gazeora::app {

// The plugins of one section as a grid of tiles: as many columns as fit, every tile in a row as
// tall as the row's tallest, measured the frame before.
class PluginGrid {
public:
    PluginGrid(PluginManagerControl& control, float scale)
        : m_card(control, scale), m_scale(scale) {}

    void Draw(const PluginCard::Fonts& fonts, const std::vector<const PluginEntry*>& entries,
              float width);

private:
    // How many tiles of at least kMinTileWidth fit across `width`.
    int ColumnsIn(float width) const;
    float RowHeight(const std::vector<const PluginEntry*>& entries, size_t first,
                    size_t count) const;
    float S(float px) const { return px * m_scale; }

    PluginCard m_card;
    const float m_scale;
    std::map<std::wstring, float> m_heights;  // each tile's, by plugin id
};

}  // namespace gazeora::app
