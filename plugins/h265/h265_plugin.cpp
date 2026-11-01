// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// The H.265 codec (H265Format) as a bundled plugin. Gazeora's own Media Foundation encoder
// encodes it.

#include "gazeora/bundled_plugin/bundled_plugin.h"
#include "gazeora/host/encode/bundled_video_codecs.h"
#include "gazeora/plugin_kit/exports/exported_codec.h"

GAZEORA_PLUGIN_EXPORT const GazeoraPluginInfoV1* GAZEORA_CALL GazeoraPluginInfo(void) {
    return gazeora::BundledPlugin::Info(GAZEORA_BUNDLED_PLUGIN_NAME);
}

GAZEORA_PLUGIN_EXPORT int32_t GAZEORA_CALL GazeoraPluginRegister(const GazeoraPluginHostV1* host) {
    static const gazeora::ExportedCodec codec(gazeora::BundledVideoCodecs::H265());
    return gazeora::BundledPlugin::RegisterCodec(host, codec.Codec());
}
