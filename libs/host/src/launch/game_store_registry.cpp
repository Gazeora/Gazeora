// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/launch/game_store_registry.h"

#include <stdexcept>

namespace gazeora {

void GameStoreRegistry::Register(std::shared_ptr<const GameStore> store) {
    if (!store) {
        throw std::invalid_argument("GameStoreRegistry: null store");
    }
    for (auto& registered : m_stores) {
        if (registered->Id() == store->Id()) {
            registered = std::move(store);
            return;
        }
    }
    m_stores.push_back(std::move(store));
}

std::shared_ptr<const GameStore> GameStoreRegistry::Find(const std::string& id) const {
    for (const auto& store : m_stores) {
        if (store->Id() == id) {
            return store;
        }
    }
    return nullptr;
}

std::shared_ptr<const GameStore> GameStoreRegistry::OwnerOf(const std::wstring& exePath,
                                                            std::wstring& appId) const {
    for (const auto& store : m_stores) {
        appId = store->AppIdFor(exePath);
        if (!appId.empty()) {
            return store;
        }
    }
    return nullptr;
}

}  // namespace gazeora
