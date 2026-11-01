// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_ui/interfaces/ui_values.h"

namespace gazeora {

// Adapter pattern: a panel's XML layout and the IUiValues behind it, as a GazeoraUiPanelV3 for
// a bundled plugin to register. The struct points back at it, so it can't be copied or moved.
class ExportedUiPanel final {
public:
    ExportedUiPanel(std::string xml, std::shared_ptr<ui::IUiValues> values);
    ExportedUiPanel(const ExportedUiPanel&) = delete;
    ExportedUiPanel& operator=(const ExportedUiPanel&) = delete;

    const GazeoraUiPanelV3& Panel() const { return m_panel; }

private:
    static ui::IUiValues& Of(void* self);
    static void GAZEORA_CALL Get(void* self, const char* key, char* value, size_t capacity);
    static void GAZEORA_CALL Set(void* self, const char* key, const char* value);
    static void GAZEORA_CALL Action(void* self, const char* action);

    const std::string m_xml;
    const std::shared_ptr<ui::IUiValues> m_values;
    GazeoraUiPanelV3 m_panel = {};
};

}  // namespace gazeora
