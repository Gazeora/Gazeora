// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_manager/models/plugin_entry.h"

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/update/models/release_version.h"

namespace gazeora {

std::wstring PluginEntry::Name() const {
    if (listed && !listed->name.empty()) {
        return listed->name;
    }
    return installed ? installed->name : id;
}

PluginCategory PluginEntry::Category() const {
    if (listed && listed->category) {
        return *listed->category;
    }
    return installed && installed->loaded ? PluginCategories::Of(*installed)
                                          : PluginCategory::Other;
}

bool PluginEntry::Installable() const {
    return listed && (listed->download || Bundled()) &&
           listed->apiVersion >= GAZEORA_PLUGIN_API_OLDEST_VERSION &&
           listed->apiVersion <= GAZEORA_PLUGIN_API_VERSION;
}

bool PluginEntry::UpdateAvailable() const {
    if (!installed || !Installable()) {
        return false;
    }
    auto have = ReleaseVersion::Parse(Utf8::FromWide(installed->version));
    auto offered = ReleaseVersion::Parse(listed->version);
    return have && offered && *have < *offered;
}

}  // namespace gazeora
