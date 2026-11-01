// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_manager/repositories/web_plugin_index.h"

#include "mappers/plugin_listing_json_mapper.h"

namespace gazeora {

std::vector<PluginListing> WebPluginIndex::Fetch() {
    return PluginListingJsonMapper(m_arch).FromIndex(m_http.Get(m_url));
}

}  // namespace gazeora
