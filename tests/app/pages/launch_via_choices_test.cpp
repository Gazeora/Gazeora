// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/launch_via_choices.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "library/fake_game_store.h"

namespace gazeora::app {
namespace {

const std::wstring kExe = L"C:\\Games\\a.exe";

class LaunchViaChoicesTest : public ::testing::Test {
protected:
    void SetUp() override {
        stores.Register(std::make_shared<FakeGameStore>("steam", kExe, L"440"));
        stores.Register(std::make_shared<FakeGameStore>("other", L"C:\\elsewhere.exe", L"7"));
        game.exePath = kExe;
    }

    GameStoreRegistry stores;
    Game game;
};

TEST_F(LaunchViaChoicesTest, ListsAutomaticTheExeAndEachStore) {
    LaunchViaChoices choices(stores, game);
    ASSERT_EQ(choices.Labels().size(), 4u);
    EXPECT_EQ(choices.Labels()[0], "Automatic (through steam, app 440)");
    EXPECT_EQ(choices.Labels()[1], "The game's exe");
    EXPECT_EQ(choices.Labels()[2], "steam");
    EXPECT_EQ(choices.Labels()[3], "other");
}

TEST_F(LaunchViaChoicesTest, ApplyAndIndexOfRoundTrip) {
    LaunchViaChoices choices(stores, game);
    for (int index = 0; index < int(choices.Labels().size()); ++index) {
        choices.Apply(index, game);
        EXPECT_EQ(choices.IndexOf(game), index);
    }
    choices.Apply(3, game);
    EXPECT_EQ(game.launchVia, LaunchVia::Store);
    EXPECT_EQ(game.storeId, "other");
    choices.Apply(1, game);
    EXPECT_EQ(game.launchVia, LaunchVia::Direct);
    EXPECT_TRUE(game.storeId.empty());
}

TEST_F(LaunchViaChoicesTest, WarnsWhenTheChosenStoreDoesNotHaveTheGame) {
    LaunchViaChoices choices(stores, game);
    choices.Apply(2, game);
    EXPECT_TRUE(choices.Warning(game).empty());
    choices.Apply(3, game);
    EXPECT_EQ(choices.Warning(game), "Not in a other library, so the exe starts directly");
}

TEST_F(LaunchViaChoicesTest, KeepsAStoreThisBuildDoesNotHave) {
    game.launchVia = LaunchVia::Store;
    game.storeId = "gone";
    LaunchViaChoices choices(stores, game);
    ASSERT_EQ(choices.Labels().size(), 5u);
    EXPECT_EQ(choices.Labels()[4], "gone (not in this version)");
    EXPECT_EQ(choices.IndexOf(game), 4);
    EXPECT_FALSE(choices.Warning(game).empty());
}

TEST_F(LaunchViaChoicesTest, AutomaticStartsTheExeWhenNoStoreHasIt) {
    game.exePath = L"C:\\nowhere.exe";
    EXPECT_EQ(LaunchViaChoices(stores, game).Labels()[0], "Automatic (start the exe)");
}

}  // namespace
}  // namespace gazeora::app
