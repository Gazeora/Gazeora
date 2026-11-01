// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <string>

namespace gazeora {

enum class LaunchVia {
    Auto,    // through the first store the exe belongs to (GameStoreRegistry), else directly
    Direct,  // run the exe
    Store,   // through the store named by Game::storeId
};

// One game in the library, as stored in library.json.
struct Game {
    uint32_t id = 0;  // stable, small, unique within the library; used by the CLI
    std::wstring name;
    std::wstring exePath;
    std::wstring arguments;
    std::wstring workingDir;  // empty = the exe's folder
    std::wstring coverPath;   // optional image shown instead of the exe icon
    bool captureOnLaunch = false;
    LaunchVia launchVia = LaunchVia::Auto;
    // With LaunchVia::Store: the store's id (GameStore::Id), e.g. "steam".
    std::string storeId;
    // The real game exe, when exePath is a launcher that starts something else. Empty =
    // exePath's file name.
    std::wstring processName;
    int64_t addedAt = 0;     // Unix seconds
    int64_t lastPlayed = 0;  // Unix seconds, 0 = never
    int64_t playtimeSeconds = 0;
};

}  // namespace gazeora
