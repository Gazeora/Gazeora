// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_api/gazeora_plugin.h"
#include "recorder_panel.h"
#include "recorder_protocol.h"
#include "recorder_settings.h"
#include "recording.h"

namespace frame_recorder {

// Facade: the whole plugin, made when Gazeora registers it and kept while the DLL is loaded,
// as the structs it registers point into it.
class FrameRecorder {
public:
    // `settingsFolder`: GazeoraPluginHostV3::settings_folder, or "" from an older Gazeora.
    explicit FrameRecorder(const std::string& settingsFolder);
    FrameRecorder(const FrameRecorder&) = delete;
    FrameRecorder& operator=(const FrameRecorder&) = delete;

    // The protocol, and the card where Gazeora has cards (plugin API version 3).
    int32_t Register(const GazeoraPluginHostV1& host) const;

private:
    RecorderSettings m_settings;
    Recording m_recording;
    RecorderProtocol m_protocol;
    RecorderPanel m_panel;
};

}  // namespace frame_recorder
