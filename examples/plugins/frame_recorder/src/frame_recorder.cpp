// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "frame_recorder.h"

#include "videos_folder.h"

namespace frame_recorder {

FrameRecorder::FrameRecorder(const std::string& settingsFolder)
    : m_settings(SettingsFile(settingsFolder), VideosFolder::Gazeora()),
      m_recording(m_settings),
      m_protocol(m_recording),
      m_panel(m_settings, m_recording) {}

int32_t FrameRecorder::Register(const GazeoraPluginHostV1& host) const {
    // Protocols came with version 2: a Gazeora with only version 1 gets nothing from this plugin.
    const GazeoraPluginHostV2* v2 = GazeoraPluginHostV2Of(&host);
    if (!v2 || v2->register_protocol(host.context, &m_protocol.Struct()) != GAZEORA_TRUE) {
        return GAZEORA_FALSE;
    }
    // Cards came with version 3. Without one, recording stays on, as it starts.
    const GazeoraPluginHostV3* v3 = GazeoraPluginHostV3Of(&host);
    if (v3) {
        return v3->register_ui_panel(host.context, &m_panel.Struct());
    }
    return GAZEORA_TRUE;
}

}  // namespace frame_recorder
