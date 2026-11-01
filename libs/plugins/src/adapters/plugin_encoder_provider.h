// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/host/encode/video_encoder_provider.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugins/interfaces/plugin_module.h"

namespace gazeora {

// Adapter pattern: a plugin's GazeoraEncoderProviderV1 behind VideoEncoderProvider. Keeps the
// DLL loaded.
class PluginEncoderProvider final : public VideoEncoderProvider {
public:
    // `provider` must have passed PluginRegistrar's checks; a version 1 one has no
    // supports_id.
    PluginEncoderProvider(std::shared_ptr<const IPluginModule> module,
                          const GazeoraEncoderProviderV2& provider);

    const std::wstring& Name() const override { return m_name; }
    bool Supports(VideoCodec codec) const override;
    // A RefusedVideoEncoder if the plugin couldn't make one.
    std::unique_ptr<VideoEncoder> Create() const override;

private:
    const std::shared_ptr<const IPluginModule> m_module;
    const GazeoraEncoderProviderV2 m_provider;
    const std::wstring m_name;
};

}  // namespace gazeora
