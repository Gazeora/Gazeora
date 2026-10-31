// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/library/services/missing_game_pruner.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gazeora/library/platform/win32_file_checker.h"

namespace gazeora {
namespace {

const std::wstring kPresentExe = L"C:\\Games\\present.exe";
const std::wstring kMissingExe = L"C:\\Games\\missing.exe";

class FakeFiles final : public IFileChecker {
public:
    bool Exists(const std::wstring& path) const override { return path == kPresentExe; }
};

class FakeRepository final : public IGameRepository {
public:
    std::vector<Game> Load() override { return {}; }
    void Save(const std::vector<Game>&) override {}
    bool ChangedSinceLastAccess() const override { return false; }
    const std::wstring& Location() const override { return m_location; }

private:
    std::wstring m_location;
};

class FixedClock final : public IClock {
public:
    int64_t NowUnixSeconds() const override { return 0; }
};

class FixedNames final : public IGameNameResolver {
public:
    std::wstring NameFor(const std::wstring&) const override { return L"game"; }
};

Game GameAt(const std::wstring& exePath) {
    Game game;
    game.exePath = exePath;
    return game;
}

TEST(MissingGamePrunerTest, RemovesOnlyGamesWhoseExeIsMissing) {
    FakeRepository repository;
    FixedClock clock;
    FixedNames names;
    GameLibrary library(repository, clock, names);
    uint32_t kept = library.Add(GameAt(kPresentExe));
    uint32_t gone = library.Add(GameAt(kMissingExe));

    FakeFiles files;
    std::vector<Game> removed = MissingGamePruner(files).Prune(library);

    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0].id, gone);
    EXPECT_NE(library.Find(kept), nullptr);
    EXPECT_EQ(library.Find(gone), nullptr);
}

TEST(Win32FileCheckerTest, FindsFilesButNotFoldersOrMissingPaths) {
    wchar_t dir[MAX_PATH];
    GetSystemDirectoryW(dir, MAX_PATH);
    Win32FileChecker files;
    EXPECT_TRUE(files.Exists(std::wstring(dir) + L"\\notepad.exe"));
    EXPECT_FALSE(files.Exists(dir));
    EXPECT_FALSE(files.Exists(kMissingExe));
}

}  // namespace
}  // namespace gazeora
