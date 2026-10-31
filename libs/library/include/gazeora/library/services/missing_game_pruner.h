// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <vector>

#include "gazeora/library/game_library.h"
#include "gazeora/library/interfaces/file_checker.h"
#include "gazeora/library/models/game.h"

namespace gazeora {

// Removes games whose exe is no longer on disk, e.g. after an uninstall.
class MissingGamePruner final {
public:
    explicit MissingGamePruner(const IFileChecker& files) : m_files(files) {}

    // Returns the games it removed; the caller saves the library.
    std::vector<Game> Prune(GameLibrary& library) const;

private:
    const IFileChecker& m_files;
};

}  // namespace gazeora
