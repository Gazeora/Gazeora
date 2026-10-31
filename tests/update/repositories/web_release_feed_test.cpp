// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/repositories/web_release_feed.h"

#include <gtest/gtest.h>

#include "gazeora/update/exceptions/update_error.h"
#include "update/fake_http_client.h"
#include "update/feed_json.h"

namespace gazeora {
namespace {

const std::string kIndex = std::string(kFeedFolder) + "index.json";
const std::string kPage1 = std::string(kFeedFolder) + "page-1.json";
const std::string kPage2 = std::string(kFeedFolder) + "page-2.json";

bool Any(const Release&) { return true; }
bool Stable(const Release& release) { return !release.Version().IsPrerelease(); }

TEST(WebReleaseFeedTest, TakesTheLatestFromTheIndexAlone) {
    FakeHttpClient http;
    http.Serve(kIndex, IndexJson(ReleaseJson("v1.1.0", {"x64", "x86"}), 1));
    WebReleaseFeed feed(http, kFeedFolder);

    std::optional<Release> newest = feed.Newest(Any);
    ASSERT_TRUE(newest);
    EXPECT_EQ(newest->Version().Label(), "v1.1.0");
    EXPECT_EQ(newest->PageUrl(), "https://example.test/tag/v1.1.0");
    const InstallerAsset* x86 = newest->InstallerFor("x86");
    ASSERT_NE(x86, nullptr);
    EXPECT_EQ(x86->Name(), "Gazeora-v1.1.0-x86.msi");
    EXPECT_EQ(x86->Url(), "https://example.test/Gazeora-v1.1.0-x86.msi");
    EXPECT_EQ(x86->Size(), 4u);
    EXPECT_EQ(x86->Sha256(), "00ff");
    EXPECT_EQ(http.Requests().size(), 1u);
}

TEST(WebReleaseFeedTest, LooksThroughThePagesWhenTheLatestIsNotWanted) {
    FakeHttpClient http;
    http.Serve(kIndex, IndexJson(ReleaseJson("v1.2.0-rc.1", {"x64"}), 2));
    http.Serve(kPage1, PageJson({ReleaseJson("v1.2.0-rc.1", {"x64"})}));
    http.Serve(kPage2, PageJson({ReleaseJson("v1.0.0", {"x64"}), ReleaseJson("v1.1.0", {"x64"})}));
    WebReleaseFeed feed(http, kFeedFolder);

    std::optional<Release> newest = feed.Newest(Stable);
    ASSERT_TRUE(newest);
    EXPECT_EQ(newest->Version().Label(), "v1.1.0");
}

TEST(WebReleaseFeedTest, FindsNothingWhenNoReleaseIsWanted) {
    FakeHttpClient http;
    http.Serve(kIndex, IndexJson(ReleaseJson("v1.2.0-rc.1", {"x64"}), 1));
    http.Serve(kPage1, PageJson({ReleaseJson("v1.2.0-rc.1", {"x64"})}));
    WebReleaseFeed feed(http, kFeedFolder);

    EXPECT_FALSE(feed.Newest(Stable));
}

TEST(WebReleaseFeedTest, SkipsTagsThatAreNotVersions) {
    FakeHttpClient http;
    http.Serve(kIndex, IndexJson(ReleaseJson("nightly", {"x64"}), 1));
    http.Serve(kPage1, PageJson({ReleaseJson("nightly", {"x64"}), ReleaseJson("v1.0.0", {})}));
    WebReleaseFeed feed(http, kFeedFolder);

    std::optional<Release> newest = feed.Newest(Any);
    ASSERT_TRUE(newest);
    EXPECT_EQ(newest->Version().Label(), "v1.0.0");
}

TEST(WebReleaseFeedTest, ReportsAFeedItCannotRead) {
    FakeHttpClient http;
    http.Serve(kIndex, "<html>not json</html>");
    WebReleaseFeed feed(http, kFeedFolder);

    EXPECT_THROW(feed.Newest(Any), UpdateError);
}

TEST(WebReleaseFeedTest, PassesOnServerErrors) {
    FakeHttpClient http;
    WebReleaseFeed feed(http, kFeedFolder);

    EXPECT_THROW(feed.Newest(Any), UpdateError);
}

}  // namespace
}  // namespace gazeora
