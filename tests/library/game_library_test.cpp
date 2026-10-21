#include "gazeora/library/game_library.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <string>

namespace gazeora {
namespace {

std::wstring SystemExe(const wchar_t* name) {
    wchar_t dir[MAX_PATH];
    GetSystemDirectoryW(dir, MAX_PATH);
    return std::wstring(dir) + L"\\" + name;
}

class GameLibraryTest : public ::testing::Test {
protected:
    void SetUp() override {
        wchar_t dir[MAX_PATH];
        GetTempPathW(MAX_PATH, dir);
        folder = std::wstring(dir) + L"gazeora_library_test_" +
                 std::to_wstring(GetCurrentProcessId());
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
    GameLibrary library(path);
    std::wstring error;
    EXPECT_TRUE(library.Load(error)) << error;
    EXPECT_TRUE(library.Games().empty());
}

TEST_F(GameLibraryTest, SaveAndLoadRoundTrip) {
    GameLibrary library(path);
    Game game;
    game.name = L"Caf\u00e9 Racer \u2014 \u65e5\u672c";  // non-ASCII survives UTF-8
    game.exePath = L"C:\\Games\\Racer\\racer.exe";
    game.arguments = L"-dx12 -windowed";
    game.workingDir = L"C:\\Games\\Racer";
    game.coverPath = L"C:\\Art\\racer.png";
    game.captureOnLaunch = true;
    game.launchVia = LaunchVia::Steam;
    game.processName = L"RacerGame.exe";
    uint32_t id = library.Add(game);
    library.Find(id)->lastPlayed = 1700000000;
    library.Find(id)->playtimeSeconds = 3725;
    std::wstring error;
    ASSERT_TRUE(library.Save(error)) << error;

    GameLibrary loaded(path);
    ASSERT_TRUE(loaded.Load(error)) << error;
    ASSERT_EQ(loaded.Games().size(), 1u);
    const Game& g = loaded.Games()[0];
    EXPECT_EQ(g.id, id);
    EXPECT_EQ(g.name, game.name);
    EXPECT_EQ(g.exePath, game.exePath);
    EXPECT_EQ(g.arguments, game.arguments);
    EXPECT_EQ(g.workingDir, game.workingDir);
    EXPECT_EQ(g.coverPath, game.coverPath);
    EXPECT_TRUE(g.captureOnLaunch);
    EXPECT_EQ(g.launchVia, LaunchVia::Steam);
    EXPECT_EQ(g.processName, L"RacerGame.exe");
    EXPECT_GT(g.addedAt, 0);
    EXPECT_EQ(g.lastPlayed, 1700000000);
    EXPECT_EQ(g.playtimeSeconds, 3725);
}

TEST_F(GameLibraryTest, AddAssignsIncreasingIdsAndDefaultName) {
    GameLibrary library(path);
    Game game;
    game.exePath = L"C:\\Games\\no_such_game.exe";
    uint32_t first = library.Add(game);
    uint32_t second = library.Add(game);
    EXPECT_EQ(first, 1u);
    EXPECT_EQ(second, 2u);
    EXPECT_EQ(library.Find(first)->name, L"no_such_game");

    // New ids follow the highest existing one.
    library.Remove(first);
    EXPECT_EQ(library.Add(game), 3u);
}

TEST_F(GameLibraryTest, DefaultNameUsesFileDescription) {
    // notepad.exe's version resource describes it as "Notepad".
    std::wstring name = DefaultGameName(SystemExe(L"notepad.exe"));
    EXPECT_FALSE(name.empty());
    EXPECT_NE(name, L"notepad") << "expected the FileDescription, not the file name";
}

TEST_F(GameLibraryTest, RemoveAndFind) {
    GameLibrary library(path);
    Game game;
    game.exePath = L"C:\\a.exe";
    game.name = L"Alpha";
    uint32_t alpha = library.Add(game);
    game.name = L"Beta";
    uint32_t beta = library.Add(game);

    EXPECT_TRUE(library.Remove(alpha));
    EXPECT_FALSE(library.Remove(alpha));
    EXPECT_EQ(library.Find(alpha), nullptr);
    ASSERT_NE(library.Find(beta), nullptr);
    EXPECT_EQ(library.Find(beta)->name, L"Beta");
}

TEST_F(GameLibraryTest, FindByIdOrName) {
    GameLibrary library(path);
    Game game;
    game.exePath = L"C:\\a.exe";
    game.name = L"Doom";
    uint32_t doom = library.Add(game);
    game.name = L"Quake";
    library.Add(game);
    game.name = L"quake";
    library.Add(game);

    EXPECT_EQ(library.FindByIdOrName(std::to_wstring(doom))->id, doom);
    EXPECT_EQ(library.FindByIdOrName(L"DOOM")->id, doom);
    EXPECT_EQ(library.FindByIdOrName(L"quake"), nullptr) << "two games match: ambiguous";
    EXPECT_EQ(library.FindByIdOrName(L"Half-Life"), nullptr);
    EXPECT_EQ(library.FindByIdOrName(L"99"), nullptr);
}

TEST_F(GameLibraryTest, LoadReportsMalformedJson) {
    WriteFile("{ \"games\": [ { \"id\": 1, ");
    GameLibrary library(path);
    std::wstring error;
    EXPECT_FALSE(library.Load(error));
    EXPECT_NE(error.find(L"could not read"), std::wstring::npos);
}

TEST_F(GameLibraryTest, LoadSkipsEntriesWithoutIdOrExe) {
    WriteFile(R"({"version": 1, "games": [
        {"id": 1, "name": "Good", "exePath": "C:\\good.exe"},
        {"id": 0, "name": "No id", "exePath": "C:\\x.exe"},
        {"id": 3, "name": "No exe"}
    ]})");
    GameLibrary library(path);
    std::wstring error;
    ASSERT_TRUE(library.Load(error)) << error;
    ASSERT_EQ(library.Games().size(), 1u);
    EXPECT_EQ(library.Games()[0].name, L"Good");
}

TEST_F(GameLibraryTest, ChangedOnDiskDetectsOtherWriters) {
    GameLibrary library(path);
    std::wstring error;
    ASSERT_TRUE(library.Save(error)) << error;
    EXPECT_FALSE(library.ChangedOnDisk());

    // Another process (e.g. GazeoraCLI) saves later.
    Sleep(20);
    GameLibrary other(path);
    Game game;
    game.exePath = L"C:\\a.exe";
    other.Add(game);
    ASSERT_TRUE(other.Save(error)) << error;
    EXPECT_TRUE(library.ChangedOnDisk());
}

TEST_F(GameLibraryTest, OlderFilesLoadWithDefaultLaunchSettings) {
    WriteFile(R"({"version": 1, "games": [{"id": 1, "name": "Old", "exePath": "C:\\old.exe"}]})");
    GameLibrary library(path);
    std::wstring error;
    ASSERT_TRUE(library.Load(error)) << error;
    ASSERT_EQ(library.Games().size(), 1u);
    EXPECT_EQ(library.Games()[0].launchVia, LaunchVia::Auto);
    EXPECT_EQ(library.Games()[0].processName, L"");
}

TEST(LaunchSpecFor, DirectGamesSkipSteam) {
    Game game;
    game.exePath = L"C:\\Games\\a.exe";
    game.arguments = L"-x";
    game.processName = L"b.exe";
    game.launchVia = LaunchVia::Direct;
    LaunchSpec spec = ToLaunchSpec(game);
    EXPECT_EQ(spec.exePath, game.exePath);
    EXPECT_EQ(spec.arguments, L"-x");
    EXPECT_EQ(spec.GameProcessName(), L"b.exe");
    EXPECT_TRUE(spec.steamAppId.empty());
}

TEST(Utf8, RoundTrip) {
    std::wstring text = L"Gr\u00fc\u00dfe \U0001F3AE";
    EXPECT_EQ(Utf8ToWide(WideToUtf8(text)), text);
    EXPECT_EQ(WideToUtf8(L""), "");
    EXPECT_EQ(WideToUtf8(L"\u00e9"), "\xC3\xA9");
}

}  // namespace
}  // namespace gazeora
