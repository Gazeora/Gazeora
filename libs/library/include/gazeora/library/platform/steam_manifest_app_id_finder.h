#pragma once

#include "gazeora/host/launch/launcher.h"
#include "gazeora/library/interfaces/steam_app_id_finder.h"

namespace gazeora {

// Adapter pattern: the host's Steam manifest lookup behind ISteamAppIdFinder.
class SteamManifestAppIdFinder final : public ISteamAppIdFinder {
public:
    std::wstring AppIdFor(const std::wstring& exePath) const override {
        return FindSteamAppId(exePath);
    }
};

}  // namespace gazeora
