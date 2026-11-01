// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <functional>
#include <optional>

#include "gazeora/update/models/release.h"

namespace gazeora {

// Which releases a caller is after.
using ReleaseFilter = std::function<bool(const Release&)>;

// Repository pattern: the published releases, newest first. Throws UpdateError.
class IReleaseFeed {
public:
    virtual ~IReleaseFeed() = default;

    // The newest release `wanted` accepts; nothing if none does.
    virtual std::optional<Release> Newest(const ReleaseFilter& wanted) = 0;
};

}  // namespace gazeora
