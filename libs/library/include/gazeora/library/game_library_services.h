// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "gazeora/host/launch/built_in_game_stores.h"
#include "gazeora/library/game_library.h"
#include "gazeora/library/platform/shell_link_resolver.h"
#include "gazeora/library/platform/system_clock.h"
#include "gazeora/library/platform/version_info_name_resolver.h"
#include "gazeora/library/platform/win32_file_checker.h"
#include "gazeora/library/repositories/json_game_repository.h"
#include "gazeora/library/services/launch_spec_factory.h"
#include "gazeora/library/services/missing_game_pruner.h"

namespace gazeora {

// Facade pattern: the library and the services around it, wired to the real system. The
// front ends' composition root. `stores` are the stores games can start through; throws
// std::invalid_argument for null.
class GameLibraryServices final {
public:
    explicit GameLibraryServices(
        std::wstring path,
        std::shared_ptr<const GameStoreRegistry> stores = BuiltInGameStores::Registry())
        : m_repository(std::move(path)),
          m_stores(Required(std::move(stores))),
          m_library(m_repository, m_clock, m_names),
          m_launchSpecs(*m_stores),
          m_pruner(m_files) {}

    GameLibraryServices(const GameLibraryServices&) = delete;
    GameLibraryServices& operator=(const GameLibraryServices&) = delete;

    GameLibrary& Library() { return m_library; }
    const LaunchSpecFactory& LaunchSpecs() const { return m_launchSpecs; }
    const GameStoreRegistry& Stores() const { return *m_stores; }
    const IGameNameResolver& Names() const { return m_names; }
    const IShortcutResolver& Shortcuts() const { return m_shortcuts; }
    const IClock& Clock() const { return m_clock; }
    const MissingGamePruner& Pruner() const { return m_pruner; }

private:
    static std::shared_ptr<const GameStoreRegistry> Required(
        std::shared_ptr<const GameStoreRegistry> stores) {
        if (!stores) {
            throw std::invalid_argument("GameLibraryServices: null store registry");
        }
        return stores;
    }

    // Declared before m_library, m_launchSpecs and m_pruner, which hold references to them.
    JsonGameRepository m_repository;
    std::shared_ptr<const GameStoreRegistry> m_stores;
    SystemClock m_clock;
    VersionInfoNameResolver m_names;
    ShellLinkResolver m_shortcuts;
    Win32FileChecker m_files;
    GameLibrary m_library;
    LaunchSpecFactory m_launchSpecs;
    MissingGamePruner m_pruner;
};

}  // namespace gazeora
