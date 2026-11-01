// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// Strategy pattern: a store's client games can be started through instead of their exe
// (Steam...). Registered in a GameStoreRegistry. Stateless, so any thread may use it.
class GameStore {
public:
    virtual ~GameStore() = default;

    // Stable, lowercase ASCII, stored in library.json: e.g. "steam".
    virtual const std::string& Id() const = 0;
    // For the UI, e.g. "Steam".
    virtual const std::wstring& Name() const = 0;
    // The store's id for the game this exe belongs to; empty if it isn't one of its games.
    virtual std::wstring AppIdFor(const std::wstring& exePath) const = 0;
    // Asks the store's client to start the game. The game process appears later, by itself.
    virtual bool Launch(const std::wstring& appId, std::wstring& error) const = 0;
};

}  // namespace gazeora
