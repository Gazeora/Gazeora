// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/services/update_checker.h"

#include <gtest/gtest.h>

#include "gazeora/update/repositories/web_release_feed.h"
#include "update/fake_http_client.h"
#include "update/feed_json.h"

namespace gazeora {
namespace {

// A feed whose latest release is `latest`, with `page` its only page.
struct FeedWith {
    FeedWith(const std::string& latest, const std::vector<std::string>& page) {
        http.Serve(std::string(kFeedFolder) + "index.json", IndexJson(latest, 1));
        http.Serve(std::string(kFeedFolder) + "page-1.json", PageJson(page));
    }

    std::optional<std::string> UpdateFor(const char* current, const char* arch = "x64") {
        UpdateChecker checker(feed, *ReleaseVersion::Parse(current), arch);
        std::optional<Release> update = checker.FindUpdate();
        return update ? std::optional<std::string>(update->Version().Label()) : std::nullopt;
    }

    FakeHttpClient http;
    WebReleaseFeed feed{http, kFeedFolder};
};

TEST(UpdateCheckerTest, OffersANewerRelease) {
    const std::string latest = ReleaseJson("v1.1.0", {"x64", "x86"});
    FeedWith feed(latest, {latest});
    EXPECT_EQ(feed.UpdateFor("v1.0.0"), "v1.1.0");
    EXPECT_EQ(feed.UpdateFor("v1.0.0", "x86"), "v1.1.0");
}

TEST(UpdateCheckerTest, OffersNothingToTheNewest) {
    const std::string latest = ReleaseJson("v1.1.0", {"x64"});
    FeedWith feed(latest, {latest, ReleaseJson("v1.0.0", {"x64"})});
    EXPECT_EQ(feed.UpdateFor("v1.1.0"), std::nullopt);
    EXPECT_EQ(feed.UpdateFor("v1.2.0"), std::nullopt);
}

TEST(UpdateCheckerTest, KeepsAReleaseOffCandidates) {
    const std::string candidate = ReleaseJson("v1.2.0-rc.1", {"x64"});
    FeedWith feed(candidate, {candidate, ReleaseJson("v1.1.0", {"x64"})});
    EXPECT_EQ(feed.UpdateFor("v1.0.0"), "v1.1.0");
    EXPECT_EQ(feed.UpdateFor("v1.1.0"), std::nullopt);
}

TEST(UpdateCheckerTest, MovesACandidateToNewerCandidatesAndItsRelease) {
    const std::string candidate = ReleaseJson("v1.2.0-rc.2", {"x64"});
    FeedWith feed(candidate, {candidate});
    EXPECT_EQ(feed.UpdateFor("v1.2.0-rc.1"), "v1.2.0-rc.2");

    const std::string release = ReleaseJson("v1.2.0", {"x64"});
    FeedWith released(release, {release, candidate});
    EXPECT_EQ(released.UpdateFor("v1.2.0-rc.2"), "v1.2.0");
}

TEST(UpdateCheckerTest, SkipsReleasesWithoutThisArchitecture) {
    const std::string latest = ReleaseJson("v1.2.0", {"x64"});
    FeedWith feed(latest, {latest, ReleaseJson("v1.1.0", {"x64", "x86"})});
    EXPECT_EQ(feed.UpdateFor("v1.0.0", "x86"), "v1.1.0");
}

}  // namespace
}  // namespace gazeora
