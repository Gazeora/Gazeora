// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// Finds the Steam app an exe belongs to.
class ISteamAppIdFinder {
public:
    virtual ~ISteamAppIdFinder() = default;

    // Empty if the exe isn't in a Steam library.
    virtual std::wstring AppIdFor(const std::wstring& exePath) const = 0;
};

}  // namespace gazeora
