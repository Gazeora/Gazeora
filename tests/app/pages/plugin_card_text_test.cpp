// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/plugin_card_text.h"

#include <gtest/gtest.h>

#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora::app {
namespace {

PluginStatus Loaded(int protocols, int codecs) {
    PluginStatus status;
    status.version = L"1.0.0";
    status.loaded = true;
    status.protocols = protocols;
    status.codecs = codecs;
    return status;
}

TEST(PluginCardTextTest, NamesOnlyWhatAPluginAdded) {
    EXPECT_EQ(PluginCardText::Contents(Loaded(1, 0)), "Adds 1 protocol");
    EXPECT_EQ(PluginCardText::Contents(Loaded(2, 1)), "Adds 2 protocols and 1 codec");
    EXPECT_EQ(PluginCardText::Contents(Loaded(0, 0)), "");
    PluginStatus controller = Loaded(0, 0);
    controller.gamepadDrivers = 1;
    EXPECT_EQ(PluginCardText::Contents(controller), "Adds 1 game controller driver");
    controller.uiPanels = 1;
    EXPECT_EQ(PluginCardText::Contents(controller), "Adds 1 game controller driver and 1 card");
}

TEST(PluginCardTextTest, ShowsTheInstalledVersionAndANewerOne) {
    PluginEntry entry;
    entry.id = L"gazeora-udp";
    entry.installed = Loaded(1, 0);
    PluginListing listing;
    listing.id = entry.id;
    listing.version = "1.1.0";
    listing.apiVersion = GAZEORA_PLUGIN_API_VERSION;
    listing.bundledFile = L"C:\\Gazeora\\bundled-plugins\\gazeora-udp.dll";
    listing.author = L"Gazeora";
    entry.listed = listing;
    EXPECT_EQ(PluginCardText::Version(entry), "1.0.0");
    EXPECT_EQ(PluginCardText::Facts(entry),
              "Adds 1 protocol  \xC2\xB7  Version 1.1.0 available");  // no author: it's Gazeora's
}

TEST(PluginCardTextTest, AnUninstalledPluginShowsTheOfferedVersion) {
    PluginEntry entry;
    PluginListing listing;
    listing.version = "2.0.0";
    entry.listed = listing;
    EXPECT_EQ(PluginCardText::Version(entry), "2.0.0");
}

}  // namespace
}  // namespace gazeora::app
