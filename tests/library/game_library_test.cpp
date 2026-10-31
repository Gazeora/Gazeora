// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/library/game_library.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "gazeora/common/text/utf8.h"
#include "gazeora/library/platform/version_info_name_resolver.h"
#include "gazeora/library/repositories/json_game_repository.h"
#include "gazeora/library/services/launch_spec_factory.h"

namespace gazeora {
namespace {

constexpr int64_t kNow = 1750000000;

class FixedClock final : public IClock {
public:
    int64_t NowUnixSeconds() const override { return kNow; }
};

class FakeSteam final : public ISteamAppIdFinder {
public:
    std::wstring AppIdFor(const std::wstring&) const override {
        ++calls;
        return L"440";
    }
    mutable int calls = 0;
};

std::wstring SystemExe(const wchar_t* name) {
    wchar_t dir[MAX_PATH];
    GetSystemDirectoryW(dir, MAX_PATH);
    return std::wstring(dir) + L"\\" + name;
}

// A library over a JSON file, with a fixed clock.
struct LibraryOnDisk {
    explicit LibraryOnDisk(const std::wstring& path) : repository(path) {}

    JsonGameRepository repository;
    FixedClock clock;
    VersionInfoNameResolver names;
    GameLibrary library{repository, clock, names};
};

class GameLibraryTest : public ::testing::Test {
protected:
    void SetUp() override {
        wchar_t dir[MAX_PATH];
        GetTempPathW(MAX_PATH, dir);
        folder =
            std::wstring(dir) + L"gazeora_library_test_" + std::to_wstring(GetCurrentProcessId());
        path = folder + L"\\library.json";
    }

    void TearDown() override {
        DeleteFileW(path.c_str());
        RemoveDirectoryW(folder.c_str());
    }

    void WriteFile(const char* text) {
        CreateDirectoryW(folder.c_str(), nullptr);
        FILE* f = _wfopen(path.c_str(), L"wb");
        ASSERT_NE(f, nullptr);
        fputs(text, f);
        fclose(f);
    }

    std::wstring folder;  // doesn't exist until saved, so Save must create it
    std::wstring path;
};

TEST_F(GameLibraryTest, MissingFileLoadsEmpty) {
    LibraryOnDisk disk(path);
    GameLibrary& lib = disk.library;
    std::wstring error;
    EXPECT_TRUE(lib.Load(error)) << error;
    EXPECT_TRUE(lib.Games().empty());
}

TEST_F(GameLibraryTest, SaveAndLoadRoundTrip) {
    LibraryOnDisk disk(path);
    GameLibrary& lib = disk.library;
    Game game;
    game.name = L"Caf\u00e9 Racer \u2014 \u65e5\u672c";  // non-ASCII survives UTF-8
    game.exePath = L"C:\\Games\\Racer\\racer.exe";
    game.arguments = L"-dx12 -windowed";
    game.workingDir = L"C:\\Games\\Racer";
    game.coverPath = L"C:\\Art\\racer.png";
    game.captureOnLaunch = true;
    game.launchVia = LaunchVia::Steam;
    game.processName = L"RacerGame.exe";
    uint32_t id = lib.Add(game);
    lib.Find(id)->lastPlayed = 1700000000;
    lib.Find(id)->playtimeSeconds = 3725;
    std::wstring error;
    ASSERT_TRUE(lib.Save(error)) << error;

    LibraryOnDisk other(path);
    ASSERT_TRUE(other.library.Load(error)) << error;
    ASSERT_EQ(other.library.Games().size(), 1u);
    const Game& g = other.library.Games()[0];
    EXPECT_EQ(g.id, id);
    EXPECT_EQ(g.name, game.name);
    EXPECT_EQ(g.exePath, game.exePath);
    EXPECT_EQ(g.arguments, game.arguments);
    EXPECT_EQ(g.workingDir, game.workingDir);
    EXPECT_EQ(g.coverPath, game.coverPath);
    EXPECT_TRUE(g.captureOnLaunch);
    EXPECT_EQ(g.launchVia, LaunchVia::Steam);
    EXPECT_EQ(g.processName, L"RacerGame.exe");
    EXPECT_EQ(g.addedAt, kNow);
    EXPECT_EQ(g.lastPlayed, 1700000000);
    EXPECT_EQ(g.playtimeSeconds, 3725);
}

TEST_F(GameLibraryTest, AddAssignsIncreasingIdsDefaultNameAndClockTime) {
    LibraryOnDisk disk(path);
    GameLibrary& lib = disk.library;
    Game game;
    game.exePath = L"C:\\Games\\no_such_game.exe";
    uint32_t first = lib.Add(game);
    uint32_t second = lib.Add(game);
    EXPECT_EQ(first, 1u);
    EXPECT_EQ(second, 2u);
    EXPECT_EQ(lib.Find(first)->name, L"no_such_game");
    EXPECT_EQ(lib.Find(first)->addedAt, kNow);

    // New ids follow the highest existing one.
    lib.Remove(first);
    EXPECT_EQ(lib.Add(game), 3u);
}

TEST(VersionInfoNameResolverTest, UsesFileDescription) {
    // notepad.exe's version resource describes it as "Notepad".
    std::wstring name = VersionInfoNameResolver().NameFor(SystemExe(L"notepad.exe"));
    EXPECT_FALSE(name.empty());
    EXPECT_NE(name, L"notepad") << "expected the FileDescription, not the file name";
}

TEST_F(GameLibraryTest, RemoveAndFind) {
    LibraryOnDisk disk(path);
    GameLibrary& lib = disk.library;
    Game game;
    game.exePath = L"C:\\a.exe";
    game.name = L"Alpha";
    uint32_t alpha = lib.Add(game);
    game.name = L"Beta";
    uint32_t beta = lib.Add(game);

    EXPECT_TRUE(lib.Remove(alpha));
    EXPECT_FALSE(lib.Remove(alpha));
    EXPECT_EQ(lib.Find(alpha), nullptr);
    ASSERT_NE(lib.Find(beta), nullptr);
    EXPECT_EQ(lib.Find(beta)->name, L"Beta");
}

TEST_F(GameLibraryTest, FindByIdOrName) {
    LibraryOnDisk disk(path);
    GameLibrary& lib = disk.library;
    Game game;
    game.exePath = L"C:\\a.exe";
    game.name = L"Doom";
    uint32_t doom = lib.Add(game);
    game.name = L"Quake";
    lib.Add(game);
    game.name = L"quake";
    lib.Add(game);

    EXPECT_EQ(lib.FindByIdOrName(std::to_wstring(doom))->id, doom);
    EXPECT_EQ(lib.FindByIdOrName(L"DOOM")->id, doom);
    EXPECT_EQ(lib.FindByIdOrName(L"quake"), nullptr) << "two games match: ambiguous";
    EXPECT_EQ(lib.FindByIdOrName(L"Half-Life"), nullptr);
    EXPECT_EQ(lib.FindByIdOrName(L"99"), nullptr);
}

TEST_F(GameLibraryTest, LoadReportsMalformedJson) {
    WriteFile("{ \"games\": [ { \"id\": 1, ");
    LibraryOnDisk disk(path);
    GameLibrary& lib = disk.library;
    std::wstring error;
    EXPECT_FALSE(lib.Load(error));
    EXPECT_NE(error.find(L"could not read"), std::wstring::npos);
    EXPECT_TRUE(lib.Games().empty());
}

TEST_F(GameLibraryTest, LoadReportsAnEmptyFile) {
    WriteFile("");
    LibraryOnDisk disk(path);
    GameLibrary& lib = disk.library;
    std::wstring error;
    EXPECT_FALSE(lib.Load(error));
}

TEST_F(GameLibraryTest, LoadSkipsEntriesWithoutIdOrExe) {
    WriteFile(R"({"version": 1, "games": [
        {"id": 1, "name": "Good", "exePath": "C:\\good.exe"},
        {"id": 0, "name": "No id", "exePath": "C:\\x.exe"},
        {"id": 3, "name": "No exe"}
    ]})");
    LibraryOnDisk disk(path);
    GameLibrary& lib = disk.library;
    std::wstring error;
    ASSERT_TRUE(lib.Load(error)) << error;
    ASSERT_EQ(lib.Games().size(), 1u);
    EXPECT_EQ(lib.Games()[0].name, L"Good");
}

TEST_F(GameLibraryTest, ChangedOnDiskDetectsOtherWriters) {
    LibraryOnDisk disk(path);
    GameLibrary& lib = disk.library;
    std::wstring error;
    ASSERT_TRUE(lib.Save(error)) << error;
    EXPECT_FALSE(lib.ChangedOnDisk());

    // Another process (e.g. GazeoraCLI) saves later.
    Sleep(20);
    LibraryOnDisk other(path);
    Game game;
    game.exePath = L"C:\\a.exe";
    other.library.Add(game);
    ASSERT_TRUE(other.library.Save(error)) << error;
    EXPECT_TRUE(lib.ChangedOnDisk());
}

TEST_F(GameLibraryTest, OlderFilesLoadWithDefaultLaunchSettings) {
    WriteFile(R"({"version": 1, "games": [{"id": 1, "name": "Old", "exePath": "C:\\old.exe"}]})");
    LibraryOnDisk disk(path);
    GameLibrary& lib = disk.library;
    std::wstring error;
    ASSERT_TRUE(lib.Load(error)) << error;
    ASSERT_EQ(lib.Games().size(), 1u);
    EXPECT_EQ(lib.Games()[0].launchVia, LaunchVia::Auto);
    EXPECT_EQ(lib.Games()[0].processName, L"");
}

TEST(LaunchSpecFactoryTest, DirectGamesSkipSteam) {
    FakeSteam steam;
    Game game;
    game.exePath = L"C:\\Games\\a.exe";
    game.arguments = L"-x";
    game.processName = L"b.exe";
    game.launchVia = LaunchVia::Direct;
    LaunchSpec spec = LaunchSpecFactory(steam).Create(game);
    EXPECT_EQ(spec.exePath, game.exePath);
    EXPECT_EQ(spec.arguments, L"-x");
    EXPECT_EQ(spec.GameProcessName(), L"b.exe");
    EXPECT_TRUE(spec.steamAppId.empty());
    EXPECT_EQ(steam.calls, 0);
}

TEST(LaunchSpecFactoryTest, AutoGamesAskSteam) {
    FakeSteam steam;
    Game game;
    game.exePath = L"C:\\Games\\a.exe";
    EXPECT_EQ(LaunchSpecFactory(steam).Create(game).steamAppId, L"440");
    EXPECT_EQ(steam.calls, 1);
}

TEST(Utf8, RoundTrip) {
    std::wstring text = L"Gr\u00fc\u00dfe \U0001F3AE";
    EXPECT_EQ(Utf8::ToWide(Utf8::FromWide(text)), text);
    EXPECT_EQ(Utf8::FromWide(L""), "");
    EXPECT_EQ(Utf8::FromWide(L"\u00e9"), "\xC3\xA9");
}

}  // namespace
}  // namespace gazeora
