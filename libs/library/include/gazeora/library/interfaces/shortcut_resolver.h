#pragma once

#include <string>

#include "gazeora/library/models/game.h"

namespace gazeora {

// Reads a shortcut the user dropped in, as a game to add.
class IShortcutResolver {
public:
    virtual ~IShortcutResolver() = default;

    // Fills the exe, arguments and working directory (and the name, if empty). False for
    // shortcuts that don't point at a file.
    virtual bool Resolve(const std::wstring& shortcutPath, Game& game) const = 0;
};

}  // namespace gazeora
