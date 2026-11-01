// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/library/models/game.h"

namespace gazeora {

// A game's launchVia as stored in library.json: "auto", "direct", or a store's id such as
// "steam" for LaunchVia::Store.
class LaunchViaNames final {
public:
    LaunchViaNames() = delete;

    static std::string NameOf(const Game& game);
    // Sets launchVia and storeId from a stored name. Empty reads as Auto.
    static void Apply(const std::string& name, Game& game);
};

}  // namespace gazeora
