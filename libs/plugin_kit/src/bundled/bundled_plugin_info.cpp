// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// The GazeoraPluginInfo export every bundled plugin has, the same but for its name. Built into
// each bundled plugin DLL by gazeora_add_bundled_plugin (cmake/BundledPlugins.cmake), not into
// gazeora_plugin_kit: a static library's unused export would be left out.

#include "gazeora/plugin_kit/bundled/bundled_plugin.h"

GAZEORA_PLUGIN_EXPORT const GazeoraPluginInfoV1* GAZEORA_CALL GazeoraPluginInfo(void) {
    return gazeora::BundledPlugin::Info(GAZEORA_BUNDLED_PLUGIN_NAME);
}
