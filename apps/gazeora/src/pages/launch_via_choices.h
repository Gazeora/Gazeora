// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

#include "gazeora/host/launch/game_store_registry.h"
#include "gazeora/library/models/game.h"

namespace gazeora::app {

// The "Launch through" choices for one game: automatic, its exe, then each store. Asks every
// store about the game once, as that reads files: build it when the page shows another game.
class LaunchViaChoices final {
public:
    LaunchViaChoices(const GameStoreRegistry& stores, const Game& game);

    // UTF-8, in order.
    const std::vector<std::string>& Labels() const { return m_labels; }
    int IndexOf(const Game& game) const;
    void Apply(int index, Game& game) const;
    // Why the game's choice falls back to its exe; empty when it doesn't.
    std::string Warning(const Game& game) const;

private:
    struct Choice {
        LaunchVia via;
        std::string storeId;
        std::wstring storeName;
        std::wstring appId;  // the game's id in the store; empty if it isn't there
        bool known = true;   // false for a store this build doesn't have
    };

    void AddStores(const GameStoreRegistry& stores, const std::wstring& exePath);
    std::string Label(const Choice& choice) const;
    std::string AutoLabel() const;

    std::vector<Choice> m_choices;
    std::vector<std::string> m_labels;
};

}  // namespace gazeora::app
