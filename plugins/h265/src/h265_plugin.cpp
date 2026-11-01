// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// The H.265 codec (H265Format) as a bundled plugin. Gazeora's own Media Foundation encoder
// encodes it. GazeoraPluginInfo comes from gazeora_add_bundled_plugin.

#include "gazeora/bundled/h265/h265_format.h"
#include "gazeora/plugin_kit/bundled/bundled_plugin.h"
#include "gazeora/plugin_kit/exports/exported_codec.h"

GAZEORA_PLUGIN_EXPORT int32_t GAZEORA_CALL GazeoraPluginRegister(const GazeoraPluginHostV1* host) {
    static const gazeora::ExportedCodec codec(gazeora::H265Format::Info());
    return gazeora::BundledPlugin::RegisterCodec(host, codec.Codec());
}
