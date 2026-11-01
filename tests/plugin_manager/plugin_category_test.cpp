// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_manager/models/plugin_category.h"

#include <gtest/gtest.h>

#include "gazeora/plugin_manager/models/plugin_entry.h"

namespace gazeora {
namespace {

PluginStatus Registered(int protocols, int codecs, int encoders, int gameStores,
                        int gamepadDrivers = 0) {
    PluginStatus status;
    status.loaded = true;
    status.protocols = protocols;
    status.codecs = codecs;
    status.encoders = encoders;
    status.gameStores = gameStores;
    status.gamepadDrivers = gamepadDrivers;
    return status;
}

TEST(PluginCategoryTest, ReadsTheListsIds) {
    EXPECT_EQ(PluginCategories::Parse("protocol"), PluginCategory::Protocol);
    EXPECT_EQ(PluginCategories::Parse("game-store"), PluginCategory::GameStore);
    EXPECT_EQ(PluginCategories::Parse("controller"), PluginCategory::Controller);
    EXPECT_EQ(PluginCategories::Parse("other"), PluginCategory::Other);
    EXPECT_FALSE(PluginCategories::Parse("Protocol").has_value());
    EXPECT_FALSE(PluginCategories::Parse("").has_value());
    EXPECT_STREQ(PluginCategories::IdOf(PluginCategory::Encoder), "encoder");
}

TEST(PluginCategoryTest, IsWhatAPluginRegisteredMostOf) {
    EXPECT_EQ(PluginCategories::Of(Registered(1, 0, 0, 0)), PluginCategory::Protocol);
    EXPECT_EQ(PluginCategories::Of(Registered(0, 1, 2, 0)), PluginCategory::Encoder);
    EXPECT_EQ(PluginCategories::Of(Registered(0, 1, 1, 0)), PluginCategory::Codec);  // a tie
    EXPECT_EQ(PluginCategories::Of(Registered(0, 0, 0, 1)), PluginCategory::GameStore);
    EXPECT_EQ(PluginCategories::Of(Registered(0, 0, 0, 0, 1)), PluginCategory::Controller);
    EXPECT_EQ(PluginCategories::Of(Registered(0, 0, 0, 1, 1)), PluginCategory::GameStore);  // a tie
    EXPECT_EQ(PluginCategories::Of(Registered(0, 0, 0, 0)), PluginCategory::Other);
}

TEST(PluginCategoryTest, AnEntryTakesTheListsWordOverWhatItRegistered) {
    PluginEntry entry;
    EXPECT_EQ(entry.Category(), PluginCategory::Other);
    entry.installed = Registered(0, 1, 0, 0);
    EXPECT_EQ(entry.Category(), PluginCategory::Codec);
    PluginListing listing;
    listing.category = PluginCategory::Encoder;
    entry.listed = listing;
    EXPECT_EQ(entry.Category(), PluginCategory::Encoder);
}

}  // namespace
}  // namespace gazeora
