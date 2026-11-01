// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/host/stream/frame_output_factory.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugins/interfaces/plugin_module.h"

namespace gazeora {

// Adapter pattern: a plugin's GazeoraProtocolV2 as a FrameOutputFactory. Keeps the DLL loaded.
class PluginOutputFactory final : public FrameOutputFactory {
public:
    // `protocol` must have passed PluginRegistrar's checks.
    PluginOutputFactory(std::shared_ptr<const IPluginModule> module,
                        const GazeoraProtocolV2& protocol);

    StreamProtocol Protocol() const override { return m_protocolId; }
    std::unique_ptr<FrameOutput> Create(StreamLog& log) const override;

private:
    const std::shared_ptr<const IPluginModule> m_module;
    const GazeoraProtocolV2 m_protocol;
    const StreamProtocol m_protocolId;
};

}  // namespace gazeora
