// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

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
