// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/library/game_library_services.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include "library/fake_game_store.h"

namespace gazeora {
namespace {

const std::wstring kExe = L"C:\\Games\\a.exe";

std::wstring LibraryPath() {
    return (std::filesystem::temp_directory_path() /
            (L"gazeora_library_services_test_" + std::to_wstring(GetCurrentProcessId()) + L".json"))
        .wstring();
}

TEST(GameLibraryServicesTest, LaunchesThroughStoresAddedWhileRunning) {
    GameLibraryServices services(LibraryPath(), std::make_shared<GameStoreRegistry>());
    Game game;
    game.exePath = kExe;
    EXPECT_FALSE(services.LaunchSpecs().Create(game).ThroughStore());

    // A plugin with a store was installed.
    auto stores = std::make_shared<GameStoreRegistry>();
    stores->Register(std::make_shared<FakeGameStore>("added", kExe, L"7"));
    services.SetStores(stores);
    const LaunchSpec spec = services.LaunchSpecs().Create(game);
    ASSERT_TRUE(spec.ThroughStore());
    EXPECT_EQ(spec.store->Id(), "added");
    EXPECT_EQ(spec.storeAppId, L"7");
    EXPECT_NE(services.Stores().Find("added"), nullptr);
}

TEST(GameLibraryServicesTest, RefusesNoStores) {
    GameLibraryServices services(LibraryPath(), std::make_shared<GameStoreRegistry>());
    EXPECT_THROW(services.SetStores(nullptr), std::invalid_argument);
}

}  // namespace
}  // namespace gazeora
