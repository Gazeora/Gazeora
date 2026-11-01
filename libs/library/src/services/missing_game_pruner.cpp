// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/library/services/missing_game_pruner.h"

namespace gazeora {

std::vector<Game> MissingGamePruner::Prune(GameLibrary& library) const {
    std::vector<Game> missing;
    for (const Game& game : library.Games()) {
        if (!m_files.Exists(game.exePath)) {
            missing.push_back(game);
        }
    }
    for (const Game& game : missing) {
        library.Remove(game.id);
    }
    return missing;
}

}  // namespace gazeora
