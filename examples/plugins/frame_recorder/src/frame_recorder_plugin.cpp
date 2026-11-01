// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// The two functions every Gazeora plugin exports (docs/plugin-development.md).

#include <memory>

#include "frame_recorder.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "guarded.h"

namespace {

constexpr char kName[] = "Frame recorder";
// MAJOR.MINOR.PATCH, as the plugin list has it, so the Plugins page can offer an update.
constexpr char kVersion[] = "1.0.0";
// The oldest plugin API version this plugin needs: protocols came with version 2. Gazeora refuses
// a plugin that reports a version newer than its own, and hands every plugin its newest host.
constexpr uint32_t kApiVersion = 2u;

const GazeoraPluginInfoV1 kInfo = {sizeof(GazeoraPluginInfoV1), kApiVersion, kName, kVersion};

// The C interface has nowhere to keep the plugin, so the DLL does, until Gazeora unloads it.
std::unique_ptr<frame_recorder::FrameRecorder> g_recorder;

}  // namespace

GAZEORA_PLUGIN_EXPORT const GazeoraPluginInfoV1* GAZEORA_CALL GazeoraPluginInfo(void) {
    return &kInfo;
}

GAZEORA_PLUGIN_EXPORT int32_t GAZEORA_CALL GazeoraPluginRegister(const GazeoraPluginHostV1* host) {
    return frame_recorder::Guarded::Call<int32_t>(GAZEORA_FALSE, [&] {
        const GazeoraPluginHostV3* v3 = GazeoraPluginHostV3Of(host);
        // settings_folder is valid only during this call: FrameRecorder copies it.
        g_recorder = std::make_unique<frame_recorder::FrameRecorder>(
            v3 && v3->settings_folder ? v3->settings_folder : "");
        return g_recorder->Register(*host);
    });
}
