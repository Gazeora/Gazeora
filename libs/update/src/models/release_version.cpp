// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/models/release_version.h"

#include <regex>
#include <tuple>

namespace gazeora {

namespace {

// The same shape the VERSION file must have (cmake/Version.cmake), with an optional "v".
const std::regex kVersionPattern(R"(^v?(\d+)\.(\d+)\.(\d+)(?:-rc\.([1-9]\d*))?$)");
constexpr char kCandidateTag[] = "-rc.";
// A release ranks above every candidate of its numbers.
constexpr int kReleaseRank = 1 << 30;
constexpr size_t kMaxNumberDigits = 6;

enum Group { kWhole, kMajor, kMinor, kPatch, kCandidate };

}  // namespace

std::optional<ReleaseVersion> ReleaseVersion::Parse(const std::string& text) {
    std::smatch match;
    if (!std::regex_match(text, match, kVersionPattern)) {
        return std::nullopt;
    }
    for (int group = kMajor; group <= kCandidate; ++group) {
        if (match[group].length() > std::ptrdiff_t(kMaxNumberDigits)) {
            return std::nullopt;  // no overflow from a strange tag
        }
    }
    const int candidate = match[kCandidate].matched ? std::stoi(match[kCandidate]) : kNoCandidate;
    return ReleaseVersion(std::stoi(match[kMajor]), std::stoi(match[kMinor]),
                          std::stoi(match[kPatch]), candidate);
}

std::string ReleaseVersion::Label() const {
    std::string label = "v" + std::to_string(m_major) + "." + std::to_string(m_minor) + "." +
                        std::to_string(m_patch);
    if (IsPrerelease()) {
        label += kCandidateTag + std::to_string(m_candidate);
    }
    return label;
}

bool ReleaseVersion::operator<(const ReleaseVersion& other) const {
    auto rank = [](const ReleaseVersion& v) {
        return std::make_tuple(v.m_major, v.m_minor, v.m_patch,
                               v.IsPrerelease() ? v.m_candidate : kReleaseRank);
    };
    return rank(*this) < rank(other);
}

bool ReleaseVersion::operator==(const ReleaseVersion& other) const {
    return !(*this < other) && !(other < *this);
}

}  // namespace gazeora
