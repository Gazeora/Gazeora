// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/host/launch/game_store.h"

namespace gazeora {

// Steam: finds a game's app id from the appmanifest_*.acf files of the Steam library its exe
// is in, and starts it with steam://rungameid/<app id>.
class SteamGameStore final : public GameStore {
public:
    const std::string& Id() const override { return m_id; }
    const std::wstring& Name() const override { return m_name; }
    std::wstring AppIdFor(const std::wstring& exePath) const override;
    bool Launch(const std::wstring& appId, std::wstring& error) const override;

private:
    // The steamapps folder `exePath` is under, and the game's folder in steamapps\common.
    // False if it isn't under one.
    static bool SplitLibraryPath(const std::wstring& exePath, std::wstring& steamapps,
                                 std::wstring& installDir);
    // The app id in one manifest when its installdir is `installDir`; empty otherwise.
    static std::wstring ManifestAppId(const std::wstring& manifestPath,
                                      const std::wstring& installDir);

    const std::string m_id = "steam";
    const std::wstring m_name = L"Steam";
};

}  // namespace gazeora
