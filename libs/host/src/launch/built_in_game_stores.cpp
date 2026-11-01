// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/launch/built_in_game_stores.h"

#include "gazeora/host/launch/steam_game_store.h"

namespace gazeora {

void BuiltInGameStores::RegisterInto(GameStoreRegistry& registry) {
    registry.Register(std::make_shared<SteamGameStore>());
}

std::shared_ptr<const GameStoreRegistry> BuiltInGameStores::Registry() {
    auto registry = std::make_shared<GameStoreRegistry>();
    RegisterInto(*registry);
    return registry;
}

}  // namespace gazeora
