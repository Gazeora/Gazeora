#pragma once

#include "gazeora/host/launch/launcher.h"
#include "gazeora/library/interfaces/steam_app_id_finder.h"
#include "gazeora/library/models/game.h"

namespace gazeora {

// Factory pattern: what to start for a game, resolving LaunchVia::Auto through Steam.
class LaunchSpecFactory final {
public:
    explicit LaunchSpecFactory(const ISteamAppIdFinder& steam) : m_steam(steam) {}

    LaunchSpec Create(const Game& game) const;

private:
    const ISteamAppIdFinder& m_steam;
};

}  // namespace gazeora
