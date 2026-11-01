// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "adapters/plugin_output_factory.h"

#include "adapters/plugin_frame_output.h"
#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

PluginOutputFactory::PluginOutputFactory(std::shared_ptr<const IPluginModule> module,
                                         const GazeoraProtocolV2& protocol)
    : m_module(std::move(module)),
      m_protocol(protocol),
      m_protocolId(PluginText::Narrow(protocol.id)) {}

std::unique_ptr<FrameOutput> PluginOutputFactory::Create(StreamLog& log) const {
    return std::make_unique<PluginFrameOutput>(m_module, m_protocol, log);
}

}  // namespace gazeora
