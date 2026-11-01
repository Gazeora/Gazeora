// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/exports/exported_protocol.h"

#include <exception>

#include "gazeora/plugin_kit/exports/exported_output_calls.h"

namespace gazeora {

ExportedProtocol::ExportedProtocol(StreamProtocolInfo info,
                                   std::shared_ptr<const FrameOutputFactory> outputs)
    : m_info(std::move(info)), m_outputs(std::move(outputs)) {
    m_protocol.struct_size = sizeof(m_protocol);
    m_protocol.self = this;
    m_protocol.id = m_info.id.Id().c_str();
    m_protocol.name = m_info.name.c_str();
    m_protocol.help = m_info.help.c_str();
    m_protocol.discovery_path = m_info.discoveryPath.c_str();
    m_protocol.opened_by_players = m_info.openedByPlayers ? GAZEORA_TRUE : GAZEORA_FALSE;
    m_protocol.create_output = CreateOutput;
}

int32_t GAZEORA_CALL ExportedProtocol::CreateOutput(void* self, const GazeoraOutputHostV2* host,
                                                    GazeoraOutputV2* output) {
    if (!host || !output) {
        return GAZEORA_FALSE;
    }
    const auto& protocol = *static_cast<const ExportedProtocol*>(self);
    // Nothing may throw out of a plugin function.
    try {
        ExportedOutputCalls::Fill(std::make_unique<ExportedOutput>(*protocol.m_outputs, *host),
                                  *output);
        return GAZEORA_TRUE;
    } catch (const std::exception&) {
        return GAZEORA_FALSE;
    }
}

}  // namespace gazeora
