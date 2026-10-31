// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "report/issue_link.h"

#include <gtest/gtest.h>

namespace gazeora::app {
namespace {

constexpr char kNewIssue[] = "https://github.com/owner/repo/issues/new";

TEST(UrlEncodeTest, KeepsUnreservedCharacters) { EXPECT_EQ(UrlEncode("Az09-_.~"), "Az09-_.~"); }

TEST(UrlEncodeTest, EscapesEverythingElse) {
    EXPECT_EQ(UrlEncode("a b\n*&#+"), "a%20b%0A%2A%26%23%2B");
}

TEST(UrlEncodeTest, EscapesEachUtf8Byte) { EXPECT_EQ(UrlEncode("\xC2\xB7"), "%C2%B7"); }

TEST(IssueLinkTest, OpensTheFormWithTheVersionAndSystem) {
    ProblemReport report("v1.2.0", "Windows 10.0.26200, x64 PC, x64 build");
    EXPECT_EQ(IssueLink(kNewIssue, "1-bug_report.yml").For(report),
              std::string(kNewIssue) +
                  "?template=1-bug_report.yml&version=v1.2.0"
                  "&system=Windows%2010.0.26200%2C%20x64%20PC%2C%20x64%20build");
}

TEST(ProblemReportTest, KeepsTheVersionAndSystem) {
    ProblemReport report("v1.2.0", "Windows 11");
    EXPECT_EQ(report.Version(), "v1.2.0");
    EXPECT_EQ(report.System(), "Windows 11");
}

}  // namespace
}  // namespace gazeora::app
