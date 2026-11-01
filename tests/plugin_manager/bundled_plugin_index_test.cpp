// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_manager/repositories/bundled_plugin_index.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_manager/exceptions/plugin_manager_error.h"

namespace gazeora {
namespace {

namespace fs = std::filesystem;

// What each bundled plugin is (plugins/CMakeLists.txt).
PluginCategory CategoryOf(const std::wstring& id) {
    if (id == L"gazeora-h265") {
        return PluginCategory::Codec;
    }
    return id == L"gazeora-controller" ? PluginCategory::Controller : PluginCategory::Protocol;
}

class BundledPluginIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        folder = fs::temp_directory_path() /
                 (L"gazeora_bundled_index_test_" + std::to_wstring(GetCurrentProcessId()));
        fs::create_directories(folder);
    }
    void TearDown() override {
        std::error_code ignored;
        fs::remove_all(folder, ignored);
    }
    void Write(const std::wstring& name, const std::string& text) const {
        std::ofstream(folder / name, std::ios::binary) << text;
    }

    fs::path folder;
};

TEST_F(BundledPluginIndexTest, ListsTheManifestsPluginsThatHaveTheirDll) {
    Write(L"bundled.json", R"({"platform": "windows", "plugins": [
        {"id": "gazeora-udp", "name": "UDP Direct", "summary": "UDP", "version": "1.0.0",
         "api_version": 2},
        {"id": "gazeora-gone", "name": "Gone", "version": "1.0.0", "api_version": 2}]})");
    Write(L"gazeora-udp.dll", "dll");
    const std::vector<PluginListing> listings = BundledPluginIndex(folder.wstring()).Fetch();
    ASSERT_EQ(listings.size(), 1u);
    EXPECT_EQ(listings[0].id, L"gazeora-udp");
    EXPECT_EQ(listings[0].name, L"UDP Direct");
    EXPECT_EQ(listings[0].bundledFile, (folder / L"gazeora-udp.dll").wstring());
    EXPECT_FALSE(listings[0].download.has_value());
}

TEST_F(BundledPluginIndexTest, NoManifestMeansNothingBundled) {
    EXPECT_TRUE(BundledPluginIndex((folder / L"missing").wstring()).Fetch().empty());
}

TEST_F(BundledPluginIndexTest, RefusesAManifestThatIsNotAPluginList) {
    Write(L"bundled.json", "not json");
    EXPECT_THROW(BundledPluginIndex(folder.wstring()).Fetch(), PluginManagerError);
}

TEST(BundledPluginsBuiltTest, TheBuildsManifestListsEveryBundledPlugin) {
    const std::vector<PluginListing> listings =
        BundledPluginIndex(Utf8::ToWide(GAZEORA_BUNDLED_PLUGIN_DIR)).Fetch();
    std::vector<std::wstring> ids;
    for (const PluginListing& listing : listings) {
        ids.push_back(listing.id);
        EXPECT_FALSE(listing.summary.empty()) << listing.id;
        EXPECT_EQ(listing.apiVersion, GAZEORA_PLUGIN_API_VERSION) << listing.id;
        EXPECT_EQ(listing.category, CategoryOf(listing.id)) << listing.id;
    }
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids, (std::vector<std::wstring>{L"gazeora-controller", L"gazeora-h265",
                                              L"gazeora-http", L"gazeora-udp", L"gazeora-usb"}));
}

}  // namespace
}  // namespace gazeora
