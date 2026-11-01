// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/host/launch/game_store_registry.h"

namespace gazeora {

// The stores Gazeora ships: Steam.
class BuiltInGameStores final {
public:
    BuiltInGameStores() = delete;

    static void RegisterInto(GameStoreRegistry& registry);
    // A registry with only the built-in stores.
    static std::shared_ptr<const GameStoreRegistry> Registry();
};

}  // namespace gazeora
