// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// The game library behind both front ends: loading and saving library.json, lookups, and
// resolving how to launch a game.
#include <windows.h>
#include <gtest/gtest.h>

#include <string>

#include "bench_util.h"
#include "gazeora/library/game_library_services.h"

namespace gazeora::bench {
namespace {

TEST(Library, LoadSaveAndLookups) {
    constexpr int kGames = 500;
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    std::wstring path = std::wstring(dir) + L"gazeora_bench_library_" +
                        std::to_wstring(GetCurrentProcessId()) + L".json";
    GameLibraryServices services(path);
    GameLibrary& library = services.Library();
    for (int i = 0; i < kGames; ++i) {
        Game game;
        game.name = L"Game number " + std::to_wstring(i);
        game.exePath = L"C:\\Games\\Game" + std::to_wstring(i) + L"\\game.exe";
        game.arguments = L"-windowed -fps 60";
        game.playtimeSeconds = i * 60;
        library.Add(game);
    }
    PrintHeader("GameLibrary, 500 games");
    std::wstring error;
    Print("Save", Measure(20, [&] { EXPECT_TRUE(library.Save(error)) << Narrow(error); }));
    GameLibraryServices loadedServices(path);
    GameLibrary& loaded = loadedServices.Library();
    Print("Load", Measure(20, [&] { EXPECT_TRUE(loaded.Load(error)) << Narrow(error); }));
    EXPECT_EQ(loaded.Games().size(), size_t(kGames));
    Print("ChangedOnDisk", Measure(200, [&] { EXPECT_FALSE(loaded.ChangedOnDisk()); }, 3, 10));
    Print("FindByIdOrName, by id",
          Measure(200, [&] { EXPECT_NE(loaded.FindByIdOrName(L"250"), nullptr); }, 3, 10));
    Print("FindByIdOrName, by name (last)",
          Measure(
              200, [&] { EXPECT_NE(loaded.FindByIdOrName(L"GAME NUMBER 499"), nullptr); }, 3, 10));
    // Auto launch looks for a Steam library around the exe, on disk.
    const Game& game = loaded.Games().back();
    Print("ToLaunchSpec (Auto: Steam lookup)", Measure(50, [&] {
              LaunchSpec spec = loadedServices.LaunchSpecs().Create(game);
              Consume(spec.exePath.data());
          }));
    DeleteFileW(path.c_str());
}

}  // namespace
}  // namespace gazeora::bench
