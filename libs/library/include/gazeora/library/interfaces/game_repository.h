// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

#include "gazeora/library/models/game.h"

namespace gazeora {

// Where the library's games are stored. Load and Save throw LibraryStorageError.
class IGameRepository {
public:
    virtual ~IGameRepository() = default;

    // A missing store loads as no games.
    virtual std::vector<Game> Load() = 0;
    virtual void Save(const std::vector<Game>& games) = 0;

    // True if someone else wrote the store since the last Load or Save.
    virtual bool ChangedSinceLastAccess() const = 0;

    // For people: where the games are kept.
    virtual const std::wstring& Location() const = 0;
};

}  // namespace gazeora
