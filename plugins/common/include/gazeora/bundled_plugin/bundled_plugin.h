// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>

#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// What every bundled plugin's two exports share: its info, with Gazeora's version, and
// registering what it offers through the version 2 or 3 host.
class BundledPlugin final {
public:
    BundledPlugin() = delete;

    // The info for a plugin called `name` (GAZEORA_BUNDLED_PLUGIN_NAME), which must be a string
    // literal.
    static const GazeoraPluginInfoV1* Info(const char* name);
    // GAZEORA_FALSE from a Gazeora without version 2, or if Gazeora refuses it.
    static int32_t RegisterProtocol(const GazeoraPluginHostV1* host,
                                    const GazeoraProtocolV2& protocol);
    static int32_t RegisterCodec(const GazeoraPluginHostV1* host, const GazeoraCodecV2& codec);
    // GAZEORA_FALSE from a Gazeora without version 3, or if Gazeora refuses it.
    static int32_t RegisterGamepadDriverProvider(const GazeoraPluginHostV1* host,
                                                 const GazeoraGamepadDriverProviderV3& provider);
    static int32_t RegisterUiPanel(const GazeoraPluginHostV1* host, const GazeoraUiPanelV3& panel);
};

}  // namespace gazeora
