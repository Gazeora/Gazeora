// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "adapters/plugin_game_store.h"

#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

PluginGameStore::PluginGameStore(std::shared_ptr<const IPluginModule> module,
                                 const GazeoraGameStoreV1& store)
    : m_module(std::move(module)),
      m_store(store),
      m_id(PluginText::Narrow(store.id)),
      m_name(PluginText::Wide(store.name)) {}

std::wstring PluginGameStore::AppIdFor(const std::wstring& exePath) const {
    PluginText appId;
    m_store.app_id_for(m_store.self, Utf8::FromWide(exePath).c_str(), appId.Data(),
                       appId.Capacity());
    return appId.Wide();
}

bool PluginGameStore::Launch(const std::wstring& appId, std::wstring& error) const {
    PluginText text;
    if (m_store.launch(m_store.self, Utf8::FromWide(appId).c_str(), text.Data(), text.Capacity()) ==
        GAZEORA_TRUE) {
        return true;
    }
    error = text.Wide();
    if (error.empty()) {
        error = L"could not start the game through " + m_name;
    }
    return false;
}

}  // namespace gazeora
