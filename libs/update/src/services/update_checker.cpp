// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/services/update_checker.h"

namespace gazeora {

std::optional<Release> UpdateChecker::FindUpdate() const {
    return m_feed.Newest([this](const Release& release) { return Wanted(release); });
}

bool UpdateChecker::Wanted(const Release& release) const {
    const bool onChannel = m_current.IsPrerelease() || !release.Version().IsPrerelease();
    return onChannel && m_current < release.Version() && release.InstallerFor(m_arch) != nullptr;
}

}  // namespace gazeora
