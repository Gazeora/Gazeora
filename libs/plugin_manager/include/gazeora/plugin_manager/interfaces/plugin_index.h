// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <vector>

#include "gazeora/plugin_manager/models/plugin_listing.h"

namespace gazeora {

// Repository pattern: the list of plugins users can install.
class IPluginIndex {
public:
    virtual ~IPluginIndex() = default;

    // Every usable plugin in the list. Throws if the list can't be read.
    virtual std::vector<PluginListing> Fetch() = 0;
};

}  // namespace gazeora
