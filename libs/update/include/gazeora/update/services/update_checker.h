// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <optional>
#include <string>

#include "gazeora/update/interfaces/release_feed.h"

namespace gazeora {

// Finds the release to update to: newer than this one, with an MSI for this build's
// architecture. A release build only moves to releases; a release candidate also to newer
// candidates, so testers keep testing.
class UpdateChecker final {
public:
    UpdateChecker(IReleaseFeed& feed, ReleaseVersion current, std::string arch)
        : m_feed(feed), m_current(current), m_arch(std::move(arch)) {}

    // Nothing if this one is the newest. Throws UpdateError.
    std::optional<Release> FindUpdate() const;

    const ReleaseVersion& Current() const { return m_current; }
    const std::string& Arch() const { return m_arch; }

private:
    bool Wanted(const Release& release) const;

    IReleaseFeed& m_feed;
    ReleaseVersion m_current;
    std::string m_arch;
};

}  // namespace gazeora
