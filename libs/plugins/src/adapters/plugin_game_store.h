// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/host/launch/game_store.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugins/interfaces/plugin_module.h"

namespace gazeora {

// Adapter pattern: a plugin's GazeoraGameStoreV1 behind GameStore. Keeps the DLL loaded.
class PluginGameStore final : public GameStore {
public:
    // `store` must have passed PluginRegistrar's checks.
    PluginGameStore(std::shared_ptr<const IPluginModule> module, const GazeoraGameStoreV1& store);

    const std::string& Id() const override { return m_id; }
    const std::wstring& Name() const override { return m_name; }
    std::wstring AppIdFor(const std::wstring& exePath) const override;
    bool Launch(const std::wstring& appId, std::wstring& error) const override;

private:
    const std::shared_ptr<const IPluginModule> m_module;
    const GazeoraGameStoreV1 m_store;
    const std::string m_id;
    const std::wstring m_name;
};

}  // namespace gazeora
