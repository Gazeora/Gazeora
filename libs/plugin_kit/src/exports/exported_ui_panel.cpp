// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/exports/exported_ui_panel.h"

#include <exception>

#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

namespace {

// Nothing may throw out of a plugin function: a failed call shows or changes nothing.
template <typename Call>
void Guarded(Call call) {
    try {
        call();
    } catch (const std::exception&) {
    }
}

}  // namespace

ExportedUiPanel::ExportedUiPanel(std::string xml, std::shared_ptr<ui::IUiValues> values)
    : m_xml(std::move(xml)), m_values(std::move(values)) {
    m_panel.struct_size = sizeof(m_panel);
    m_panel.self = this;
    m_panel.xml = m_xml.c_str();
    m_panel.get = Get;
    m_panel.set = Set;
    m_panel.action = Action;
}

ui::IUiValues& ExportedUiPanel::Of(void* self) {
    return *static_cast<ExportedUiPanel*>(self)->m_values;
}

void GAZEORA_CALL ExportedUiPanel::Get(void* self, const char* key, char* value, size_t capacity) {
    PluginText::Write("", value, capacity);
    Guarded([&] { PluginText::Write(Of(self).Get(PluginText::Narrow(key)), value, capacity); });
}

void GAZEORA_CALL ExportedUiPanel::Set(void* self, const char* key, const char* value) {
    Guarded([&] { Of(self).Set(PluginText::Narrow(key), PluginText::Narrow(value)); });
}

void GAZEORA_CALL ExportedUiPanel::Action(void* self, const char* action) {
    Guarded([&] { Of(self).Act(PluginText::Narrow(action)); });
}

}  // namespace gazeora
