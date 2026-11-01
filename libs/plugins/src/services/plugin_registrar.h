// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugins/interfaces/plugin_module.h"
#include "gazeora/plugins/models/plugin_catalog.h"

namespace gazeora {

// What one plugin registers during GazeoraPluginRegister: checks each struct and wraps it in
// its adapter. The Host() it hands out points at this object, so keep it alive for the call.
class PluginRegistrar final {
public:
    explicit PluginRegistrar(std::shared_ptr<const IPluginModule> module)
        : m_module(std::move(module)) {}
    PluginRegistrar(const PluginRegistrar&) = delete;
    PluginRegistrar& operator=(const PluginRegistrar&) = delete;

    // The version 2 host; a version 1 plugin reads its first part only.
    GazeoraPluginHostV2 Host();

    // What was accepted, without the statuses.
    const PluginCatalog& Registered() const { return m_registered; }
    // Why each refused registration was refused.
    const std::vector<std::wstring>& Refusals() const { return m_refusals; }

private:
    static int32_t GAZEORA_CALL OnGameStore(void* context, const GazeoraGameStoreV1* store);
    static int32_t GAZEORA_CALL OnEncoderProvider(void* context,
                                                  const GazeoraEncoderProviderV1* provider);
    static int32_t GAZEORA_CALL OnCodec(void* context, const GazeoraCodecV2* codec);
    static int32_t GAZEORA_CALL OnProtocol(void* context, const GazeoraProtocolV2* protocol);
    int32_t AddGameStore(const GazeoraGameStoreV1* store);
    int32_t AddEncoderProvider(const GazeoraEncoderProviderV1* provider);
    int32_t AddCodec(const GazeoraCodecV2* codec);
    int32_t AddProtocol(const GazeoraProtocolV2* protocol);
    int32_t Refuse(std::wstring why);

    const std::shared_ptr<const IPluginModule> m_module;
    PluginCatalog m_registered;
    std::vector<std::wstring> m_refusals;
};

}  // namespace gazeora
