// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <optional>
#include <string>

namespace gazeora {

// A Gazeora version: v1.2.0, or v1.2.0-rc.3 for a release candidate. A release candidate
// comes before the release of the same numbers.
class ReleaseVersion final {
public:
    // From "v1.2.0", "1.2.0" or "v1.2.0-rc.3"; nothing if it's none of those.
    static std::optional<ReleaseVersion> Parse(const std::string& text);

    bool IsPrerelease() const { return m_candidate != kNoCandidate; }
    // How Gazeora shows it: v1.2.0, or v1.2.0-rc.3.
    std::string Label() const;

    bool operator<(const ReleaseVersion& other) const;
    bool operator==(const ReleaseVersion& other) const;

private:
    static constexpr int kNoCandidate = 0;

    ReleaseVersion(int major, int minor, int patch, int candidate)
        : m_major(major), m_minor(minor), m_patch(patch), m_candidate(candidate) {}

    int m_major;
    int m_minor;
    int m_patch;
    int m_candidate;  // the N of -rc.N; kNoCandidate for a release
};

}  // namespace gazeora
