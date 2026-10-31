// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <optional>

#include <nlohmann/json.hpp>

#include "gazeora/update/models/release.h"

namespace gazeora {

// Mapper from a release in the website's release feed (data/releases/windows/) to a Release.
class ReleaseJsonMapper final {
public:
    // Nothing if its tag isn't a Gazeora version.
    std::optional<Release> FromJson(const nlohmann::json& entry) const;

private:
    InstallerAsset AssetFromJson(const nlohmann::json& entry) const;
};

}  // namespace gazeora
