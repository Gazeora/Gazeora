// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/host/launch/game_store_registry.h"
#include "gazeora/host/launch/launcher.h"
#include "gazeora/library/models/game.h"

namespace gazeora {

// Factory pattern: what to start for a game, resolving its store through the registry. A game
// its store doesn't know (or a store that isn't registered) starts from its exe.
class LaunchSpecFactory final {
public:
    explicit LaunchSpecFactory(const GameStoreRegistry& stores) : m_stores(stores) {}

    LaunchSpec Create(const Game& game) const;

private:
    // Sets the spec's store and app id, if the game starts through one.
    void ResolveStore(const Game& game, LaunchSpec& spec) const;

    const GameStoreRegistry& m_stores;
};

}  // namespace gazeora
