// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/models/release.h"

namespace gazeora {

namespace {

// Gazeora-v1.2.0-x64.msi: the release workflow names each MSI after its architecture.
std::string InstallerSuffix(const std::string& arch) { return "-" + arch + ".msi"; }

bool EndsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

const InstallerAsset* Release::InstallerFor(const std::string& arch) const {
    const std::string suffix = InstallerSuffix(arch);
    for (const InstallerAsset& asset : m_assets) {
        if (EndsWith(asset.Name(), suffix)) {
            return &asset;
        }
    }
    return nullptr;
}

}  // namespace gazeora
