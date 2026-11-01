// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <memory>

#include "gazeora/host/stream/simple_frame_output_factory.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_kit/exports/exported_protocol.h"

namespace gazeora {

// What every bundled plugin's two exports share: its info, with Gazeora's version, and
// registering what it offers through the version 2 or 3 host. Each Register* returns
// GAZEORA_FALSE from a Gazeora without that version, or if Gazeora refuses it.
class BundledPlugin final {
public:
    BundledPlugin() = delete;

    // The info for a plugin called `name` (GAZEORA_BUNDLED_PLUGIN_NAME), which must be a string
    // literal.
    static const GazeoraPluginInfoV1* Info(const char* name);
    static int32_t RegisterProtocol(const GazeoraPluginHostV1* host,
                                    const GazeoraProtocolV2& protocol);
    static int32_t RegisterCodec(const GazeoraPluginHostV1* host, const GazeoraCodecV2& codec);
    static int32_t RegisterGamepadDriverProvider(const GazeoraPluginHostV1* host,
                                                 const GazeoraGamepadDriverProviderV3& provider);
    static int32_t RegisterUiPanel(const GazeoraPluginHostV1* host, const GazeoraUiPanelV3& panel);

    // A protocol whose FrameOutput is `Output`, described by Output::Info(): what the UDP,
    // HTTP and USB plugins register. Its struct lives as long as the DLL.
    template <typename Output>
    static int32_t RegisterProtocolOf(const GazeoraPluginHostV1* host) {
        static const ExportedProtocol protocol(
            Output::Info(), std::make_shared<SimpleFrameOutputFactory<Output>>(Output::Info().id));
        return RegisterProtocol(host, protocol.Protocol());
    }
};

}  // namespace gazeora
