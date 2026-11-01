// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "gazeora/plugin_manager/exceptions/plugin_manager_error.h"
#include "gazeora/plugin_manager/models/plugin_id.h"
#include "gazeora/plugin_manager/repositories/web_plugin_index.h"

namespace gazeora {
namespace {

constexpr char kSha[] = "AB12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34";

// Serves one body for every URL.
class FakeHttp final : public IHttpClient {
public:
    explicit FakeHttp(std::string body) : m_body(std::move(body)) {}
    std::string Get(const std::string& url) override {
        lastUrl = url;
        return m_body;
    }
    void Download(const std::string&, const std::wstring&, const DownloadProgress&) override {}

    std::string lastUrl;

private:
    const std::string m_body;
};

std::vector<PluginListing> Fetch(const std::string& body, const std::string& arch = "x64") {
    FakeHttp http(body);
    return WebPluginIndex(http, "https://example.test/index.json", arch).Fetch();
}

const char kIndex[] = R"({"platform": "windows", "plugins": [
  {"id": "example-store", "name": "Example store", "summary": "Starts example games",
   "author": "Someone", "license": "MIT", "homepage": "https://example.test",
   "version": "1.2.0", "api_version": 1, "category": "game-store",
   "assets": {"x64": {"name": "whatever.dll", "size": 4096,
                      "browser_download_url": "https://example.test/x64.dll",
                      "digest": "sha256:AB12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34"}}},
  {"id": "Bad Id", "name": "Bad", "version": "1.0.0"},
  {"id": "no-version", "name": "No version"},
  {"id": "plain-http", "name": "Plain", "version": "1.0.0", "homepage": "http://example.test",
   "category": "teleporter",
   "assets": {"x64": {"browser_download_url": "http://example.test/x.dll",
                      "digest": "sha256:AB12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34ab12cd34"}}},
  "not an object"
]})";

TEST(WebPluginIndexTest, ReadsUsableEntriesForThisArchitecture) {
    std::vector<PluginListing> listings = Fetch(kIndex);
    ASSERT_EQ(listings.size(), 2u);
    const PluginListing& store = listings[0];
    EXPECT_EQ(store.id, L"example-store");
    EXPECT_EQ(store.name, L"Example store");
    EXPECT_EQ(store.summary, L"Starts example games");
    EXPECT_EQ(store.author, L"Someone");
    EXPECT_EQ(store.license, L"MIT");
    EXPECT_EQ(store.homepage, "https://example.test");
    EXPECT_EQ(store.version, "1.2.0");
    EXPECT_EQ(store.apiVersion, 1u);
    EXPECT_EQ(store.category, PluginCategory::GameStore);
    ASSERT_TRUE(store.download.has_value());
    EXPECT_EQ(store.download->Name(), "example-store.dll");  // named after the id
    EXPECT_EQ(store.download->Url(), "https://example.test/x64.dll");
    EXPECT_EQ(store.download->Size(), 4096u);
    EXPECT_EQ(store.download->Sha256(), std::string(kSha).replace(0, 2, "ab"));
}

TEST(WebPluginIndexTest, DropsInsecureDownloadsAndHomepages) {
    std::vector<PluginListing> listings = Fetch(kIndex);
    ASSERT_EQ(listings.size(), 2u);
    EXPECT_EQ(listings[1].id, L"plain-http");
    EXPECT_FALSE(listings[1].download.has_value());
    EXPECT_TRUE(listings[1].homepage.empty());
    EXPECT_FALSE(listings[1].category.has_value());  // not a category Gazeora knows
}

TEST(WebPluginIndexTest, OtherArchitectureHasNoDownload) {
    std::vector<PluginListing> listings = Fetch(kIndex, "x86");
    ASSERT_FALSE(listings.empty());
    EXPECT_FALSE(listings[0].download.has_value());
}

TEST(WebPluginIndexTest, RefusesSomethingThatIsNoList) {
    EXPECT_THROW(Fetch("<html>"), PluginManagerError);
    EXPECT_THROW(Fetch(R"({"plugins": 3})"), PluginManagerError);
    EXPECT_TRUE(Fetch(R"({"plugins": []})").empty());
}

TEST(PluginIdTest, ValidatesAndDerivesIds) {
    EXPECT_TRUE(PluginId::IsValid(L"example-store_2"));
    EXPECT_FALSE(PluginId::IsValid(L""));
    EXPECT_FALSE(PluginId::IsValid(L"Example"));
    EXPECT_FALSE(PluginId::IsValid(L"a/b"));
    EXPECT_FALSE(PluginId::IsValid(std::wstring(65, L'a')));
    EXPECT_EQ(PluginId::OfPath(L"C:\\x\\plugins\\My_Plugin.DLL"), L"my_plugin");
    EXPECT_EQ(PluginId::ForPickedFile(L"D:\\Downloads\\Cool Store (1).dll"), L"cool-store--1-");
    EXPECT_EQ(PluginId::ForPickedFile(L"D:\\(((.dll"), L"");
}

TEST(PluginIdTest, AcceptsOnlyBareDllNames) {
    EXPECT_TRUE(PluginId::IsSafeDllName(L"example.dll"));
    EXPECT_TRUE(PluginId::IsSafeDllName(L"Example.DLL"));
    EXPECT_FALSE(PluginId::IsSafeDllName(L"example.exe"));
    EXPECT_FALSE(PluginId::IsSafeDllName(L".dll"));
    EXPECT_FALSE(PluginId::IsSafeDllName(L"..\\gazeora.dll"));
    EXPECT_FALSE(PluginId::IsSafeDllName(L"C:\\Windows\\x.dll"));
    EXPECT_FALSE(PluginId::IsSafeDllName(L"a/b.dll"));
}

}  // namespace
}  // namespace gazeora
