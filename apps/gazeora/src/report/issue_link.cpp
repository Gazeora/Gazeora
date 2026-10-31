// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "report/issue_link.h"

#include <cctype>
#include <cstdio>

namespace gazeora::app {

namespace {

// The issue form's field ids
constexpr char kVersionField[] = "version";
constexpr char kSystemField[] = "system";

bool Unreserved(unsigned char c) {
    return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
}

std::string Parameter(const std::string& name, const std::string& value) {
    return name + "=" + UrlEncode(value);
}

}  // namespace

std::string IssueLink::For(const ProblemReport& report) const {
    return m_newIssueUrl + "?" + Parameter("template", m_template) + "&" +
           Parameter(kVersionField, report.Version()) + "&" +
           Parameter(kSystemField, report.System());
}

std::string UrlEncode(const std::string& text) {
    std::string encoded;
    encoded.reserve(text.size() * 3);
    for (unsigned char c : text) {
        if (Unreserved(c)) {
            encoded += static_cast<char>(c);
        } else {
            char escape[4];
            std::snprintf(escape, sizeof(escape), "%%%02X", c);
            encoded += escape;
        }
    }
    return encoded;
}

}  // namespace gazeora::app
