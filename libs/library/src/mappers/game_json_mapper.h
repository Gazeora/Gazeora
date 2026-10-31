// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <nlohmann/json.hpp>

#include "gazeora/library/models/game.h"

namespace gazeora {

// Mapper between a Game and its library.json entry.
class GameJsonMapper final {
public:
    nlohmann::json ToJson(const Game& game) const;
    // Missing fields take their defaults, so older files still load.
    Game FromJson(const nlohmann::json& entry) const;
};

}  // namespace gazeora
