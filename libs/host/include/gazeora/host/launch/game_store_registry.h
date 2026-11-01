// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/launch/game_store.h"

namespace gazeora {

// Registry pattern: the stores games can be started through, in the order they're offered
// and asked. Fill it before handing it out; it's read only after that.
class GameStoreRegistry final {
public:
    // Replaces the store with the same id, keeping its place; otherwise added last. Throws
    // std::invalid_argument for null.
    void Register(std::shared_ptr<const GameStore> store);

    const std::vector<std::shared_ptr<const GameStore>>& Stores() const { return m_stores; }
    // Null if no store has this id.
    std::shared_ptr<const GameStore> Find(const std::string& id) const;
    // The first store the exe belongs to, with the game's id there in `appId`; null if none.
    std::shared_ptr<const GameStore> OwnerOf(const std::wstring& exePath,
                                             std::wstring& appId) const;

private:
    std::vector<std::shared_ptr<const GameStore>> m_stores;
};

}  // namespace gazeora
