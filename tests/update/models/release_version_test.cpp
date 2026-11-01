// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/models/release_version.h"

#include <gtest/gtest.h>

namespace gazeora {
namespace {

ReleaseVersion V(const char* text) { return *ReleaseVersion::Parse(text); }

TEST(ReleaseVersionTest, ReadsReleasesAndCandidates) {
    EXPECT_EQ(V("v1.2.0").Label(), "v1.2.0");
    EXPECT_EQ(V("1.2.0").Label(), "v1.2.0");
    EXPECT_EQ(V("v1.2.0-rc.3").Label(), "v1.2.0-rc.3");
    EXPECT_FALSE(V("v1.2.0").IsPrerelease());
    EXPECT_TRUE(V("v1.2.0-rc.3").IsPrerelease());
}

TEST(ReleaseVersionTest, RefusesOtherTags) {
    for (const char* tag :
         {"", "v1.2", "v1.2.0-beta", "v1.2.0-rc.0", "1.2.0 ", "nightly", "v9999999.0.0"}) {
        EXPECT_FALSE(ReleaseVersion::Parse(tag)) << tag;
    }
}

TEST(ReleaseVersionTest, OrdersByNumbersThenCandidates) {
    EXPECT_LT(V("v1.0.0"), V("v1.0.1"));
    EXPECT_LT(V("v1.9.0"), V("v1.10.0"));
    EXPECT_LT(V("v1.0.1-rc.1"), V("v1.0.1-rc.2"));
    EXPECT_LT(V("v1.0.1-rc.9"), V("v1.0.1-rc.10"));
    EXPECT_LT(V("v1.0.1-rc.2"), V("v1.0.1"));
    EXPECT_LT(V("v1.0.0"), V("v1.0.1-rc.1"));
    EXPECT_EQ(V("v1.0.1"), V("1.0.1"));
}

}  // namespace
}  // namespace gazeora
