#include "gazeora/library/services/launch_spec_factory.h"

namespace gazeora {

LaunchSpec LaunchSpecFactory::Create(const Game& game) const {
    LaunchSpec spec;
    spec.exePath = game.exePath;
    spec.arguments = game.arguments;
    spec.workingDir = game.workingDir;
    spec.processName = game.processName;
    if (game.launchVia != LaunchVia::Direct) {
        spec.steamAppId = m_steam.AppIdFor(game.exePath);
    }
    return spec;
}

}  // namespace gazeora
