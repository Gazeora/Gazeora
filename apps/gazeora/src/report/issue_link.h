// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <utility>

#include "report/problem_report.h"

namespace gazeora::app {

inline constexpr char kGazeoraNewIssue[] = "https://github.com/Gazeora/Gazeora/issues/new";
// The organization's bug report issue form (Gazeora/.github).
inline constexpr char kBugReportTemplate[] = "1-bug_report.yml";

// A link to GitHub's new issue page that opens an issue form with a report's fields filled
// in. Blank issues are turned off, so a plain body would be dropped on the way to the form.
class IssueLink final {
public:
    IssueLink(std::string newIssueUrl, std::string issueTemplate)
        : m_newIssueUrl(std::move(newIssueUrl)), m_template(std::move(issueTemplate)) {}

    std::string For(const ProblemReport& report) const;

private:
    std::string m_newIssueUrl;
    std::string m_template;
};

// Percent-encodes UTF-8 `text` for a URL's query, leaving only RFC 3986's unreserved
// characters as they are.
std::string UrlEncode(const std::string& text);

}  // namespace gazeora::app
