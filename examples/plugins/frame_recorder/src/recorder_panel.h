// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <functional>
#include <map>
#include <string>

#include "gazeora/plugin_api/gazeora_plugin.h"
#include "recorder_settings.h"
#include "recording.h"

namespace frame_recorder {

// Adapter pattern: the card on the Settings page (ui/recorder_panel.xml), its keys and actions
// onto the settings and the recording. Gazeora calls it on its UI thread, while it's shown.
class RecorderPanel {
public:
    RecorderPanel(RecorderSettings& settings, Recording& recording);
    // The struct points back at this object, so it stays where it is.
    RecorderPanel(const RecorderPanel&) = delete;
    RecorderPanel& operator=(const RecorderPanel&) = delete;

    const GazeoraUiPanelV3& Struct() const;

private:
    using Getter = std::function<std::string()>;
    using Setter = std::function<void(const std::string&)>;

    static void GAZEORA_CALL Get(void* self, const char* key, char* value, size_t capacity);
    static void GAZEORA_CALL Set(void* self, const char* key, const char* value);
    static void GAZEORA_CALL Action(void* self, const char* action);

    std::map<std::string, Getter> Getters();
    std::map<std::string, Setter> Setters();

    RecorderSettings& m_settings;
    Recording& m_recording;
    const std::map<std::string, Getter> m_getters;
    const std::map<std::string, Setter> m_setters;
    GazeoraUiPanelV3 m_panel;
};

}  // namespace frame_recorder
