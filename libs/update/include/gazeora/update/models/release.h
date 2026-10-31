// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "gazeora/update/models/installer_asset.h"
#include "gazeora/update/models/release_version.h"

namespace gazeora {

// A published Gazeora release for Windows, with its files.
class Release final {
public:
    Release(ReleaseVersion version, std::string pageUrl, std::vector<InstallerAsset> assets)
        : m_version(version), m_pageUrl(std::move(pageUrl)), m_assets(std::move(assets)) {}

    const ReleaseVersion& Version() const { return m_version; }
    // The release's page, with what changed.
    const std::string& PageUrl() const { return m_pageUrl; }
    // The MSI for `arch` (x64 or x86); nullptr if the release has none.
    const InstallerAsset* InstallerFor(const std::string& arch) const;

private:
    ReleaseVersion m_version;
    std::string m_pageUrl;
    std::vector<InstallerAsset> m_assets;
};

}  // namespace gazeora
