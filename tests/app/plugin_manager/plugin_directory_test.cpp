// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_manager/services/plugin_directory.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {
namespace {

PluginStatus Installed(const std::wstring& file, const std::wstring& name,
                       const std::wstring& version) {
    PluginStatus status;
    status.path = L"C:\\Gazeora\\bin\\plugins\\" + file;
    status.name = name;
    status.version = version;
    status.loaded = true;
    return status;
}

PluginListing Listed(const std::wstring& id, const std::wstring& name, const std::string& version,
                     bool download = true) {
    PluginListing listing;
    listing.id = id;
    listing.name = name;
    listing.summary = L"Adds the " + name + L" store";
    listing.author = L"Ada";
    listing.version = version;
    listing.apiVersion = GAZEORA_PLUGIN_API_VERSION;
    if (download) {
        listing.download.emplace(Utf8::FromWide(id) + ".dll", "https://x.test/a", 0, "ab");
    }
    return listing;
}

TEST(PluginDirectoryTest, JoinsByIdInstalledFirst) {
    std::vector<PluginEntry> entries = PluginDirectory::Join(
        {Installed(L"Zeta.dll", L"Zeta", L"1.0.0"), Installed(L"local.dll", L"Local", L"0.1")},
        {Listed(L"alpha", L"Alpha", "2.0.0"), Listed(L"zeta", L"Zeta store", "1.1.0")});
    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].id, L"local");
    EXPECT_FALSE(entries[0].listed.has_value());
    EXPECT_EQ(entries[1].id, L"zeta");
    EXPECT_TRUE(entries[1].installed.has_value());
    EXPECT_EQ(entries[1].Name(), L"Zeta store");  // the list's name wins
    EXPECT_EQ(entries[2].id, L"alpha");
    EXPECT_FALSE(entries[2].installed.has_value());
}

TEST(PluginDirectoryTest, UpdateOnlyForANewerInstallableVersion) {
    PluginEntry entry;
    entry.installed = Installed(L"a.dll", L"A", L"1.0.0");
    entry.listed = Listed(L"a", L"A", "1.1.0");
    EXPECT_TRUE(entry.UpdateAvailable());
    entry.listed->version = "1.0.0";
    EXPECT_FALSE(entry.UpdateAvailable());
    entry.listed->version = "0.9.0";
    EXPECT_FALSE(entry.UpdateAvailable());
    entry.listed->version = "2.0.0";
    entry.listed->apiVersion = GAZEORA_PLUGIN_API_VERSION + 1;
    EXPECT_FALSE(entry.Installable());
    EXPECT_FALSE(entry.UpdateAvailable());
    entry.listed->apiVersion = GAZEORA_PLUGIN_API_OLDEST_VERSION;
    EXPECT_TRUE(entry.Installable());  // an older version still loads
    entry.listed = Listed(L"a", L"A", "2.0.0", false);
    EXPECT_FALSE(entry.UpdateAvailable());
    entry.listed = Listed(L"a", L"A", "2.0.0");
    entry.installed->version = L"unknown";
    EXPECT_FALSE(entry.UpdateAvailable());
}

TEST(PluginDirectoryTest, SearchesNameSummaryAuthorAndId) {
    PluginEntry entry;
    entry.id = L"epic-store";
    entry.listed = Listed(L"epic-store", L"Epic", "1.0.0");
    EXPECT_TRUE(PluginDirectory::Matches(entry, L""));
    EXPECT_TRUE(PluginDirectory::Matches(entry, L"EPIC"));
    EXPECT_TRUE(PluginDirectory::Matches(entry, L"store"));
    EXPECT_TRUE(PluginDirectory::Matches(entry, L"ada"));
    EXPECT_FALSE(PluginDirectory::Matches(entry, L"encoder"));
}

}  // namespace
}  // namespace gazeora
