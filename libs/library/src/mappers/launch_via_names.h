#pragma once

#include <string>

#include "gazeora/library/models/game.h"

namespace gazeora {

// LaunchVia as stored in library.json: "auto", "direct" or "steam".
class LaunchViaNames final {
public:
    LaunchViaNames() = delete;

    static const char* NameOf(LaunchVia via);
    // Unknown names read as Auto.
    static LaunchVia FromName(const std::string& name);
};

}  // namespace gazeora
