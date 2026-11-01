// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/library/services/launch_spec_factory.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "library/fake_game_store.h"

namespace gazeora {
namespace {

const std::wstring kExe = L"C:\\Games\\a.exe";
const std::wstring kOtherExe = L"C:\\Games\\b.exe";

class LaunchSpecFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        first = std::make_shared<FakeGameStore>("first", kOtherExe, L"1");
        second = std::make_shared<FakeGameStore>("second", kExe, L"440");
        stores.Register(first);
        stores.Register(second);
        game.exePath = kExe;
    }

    LaunchSpec Create() const { return LaunchSpecFactory(stores).Create(game); }

    std::shared_ptr<FakeGameStore> first, second;
    GameStoreRegistry stores;
    Game game;
};

TEST_F(LaunchSpecFactoryTest, DirectGamesAskNoStore) {
    game.arguments = L"-x";
    game.processName = L"b.exe";
    game.launchVia = LaunchVia::Direct;
    LaunchSpec spec = Create();
    EXPECT_EQ(spec.exePath, kExe);
    EXPECT_EQ(spec.arguments, L"-x");
    EXPECT_EQ(spec.GameProcessName(), L"b.exe");
    EXPECT_FALSE(spec.ThroughStore());
    EXPECT_EQ(first->Lookups() + second->Lookups(), 0);
}

TEST_F(LaunchSpecFactoryTest, AutoGamesGoThroughTheStoreThatOwnsThem) {
    LaunchSpec spec = Create();
    ASSERT_TRUE(spec.ThroughStore());
    EXPECT_EQ(spec.store->Id(), "second");
    EXPECT_EQ(spec.storeAppId, L"440");
}

TEST_F(LaunchSpecFactoryTest, AutoGamesNoStoreOwnsStartTheExe) {
    game.exePath = L"C:\\Games\\elsewhere.exe";
    EXPECT_FALSE(Create().ThroughStore());
}

TEST_F(LaunchSpecFactoryTest, ChosenStoreIsUsedWhenItHasTheGame) {
    game.launchVia = LaunchVia::Store;
    game.storeId = "second";
    LaunchSpec spec = Create();
    ASSERT_TRUE(spec.ThroughStore());
    EXPECT_EQ(spec.store->Id(), "second");
    EXPECT_EQ(first->Lookups(), 0);
}

TEST_F(LaunchSpecFactoryTest, ChosenStoreWithoutTheGameStartsTheExe) {
    game.launchVia = LaunchVia::Store;
    game.storeId = "first";
    EXPECT_FALSE(Create().ThroughStore());
}

TEST_F(LaunchSpecFactoryTest, UnknownStoreStartsTheExe) {
    game.launchVia = LaunchVia::Store;
    game.storeId = "missing";
    EXPECT_FALSE(Create().ThroughStore());
}

TEST(GameStoreRegistryTest, RegisteringTheSameIdReplacesInPlace) {
    GameStoreRegistry stores;
    stores.Register(std::make_shared<FakeGameStore>("a", kExe, L"1"));
    stores.Register(std::make_shared<FakeGameStore>("b", kExe, L"2"));
    stores.Register(std::make_shared<FakeGameStore>("a", kExe, L"3"));
    ASSERT_EQ(stores.Stores().size(), 2u);
    std::wstring appId;
    EXPECT_EQ(stores.OwnerOf(kExe, appId)->Id(), "a");
    EXPECT_EQ(appId, L"3");
    EXPECT_EQ(stores.Find("b")->AppIdFor(kExe), L"2");
    EXPECT_EQ(stores.Find("c"), nullptr);
}

TEST(GameStoreRegistryTest, RefusesNull) {
    GameStoreRegistry stores;
    EXPECT_THROW(stores.Register(nullptr), std::invalid_argument);
}

}  // namespace
}  // namespace gazeora
