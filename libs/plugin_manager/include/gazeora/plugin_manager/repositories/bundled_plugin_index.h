// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

#include "gazeora/plugin_manager/interfaces/plugin_index.h"

namespace gazeora {

// The plugins that come with Gazeora (PluginFolder::Bundled): the folder's bundled.json, in the
// plugin list's format, with each entry's DLL beside it. They install from that DLL, without a
// download. Entries whose DLL is missing are left out.
class BundledPluginIndex final : public IPluginIndex {
public:
    static constexpr wchar_t kManifestName[] = L"bundled.json";

    explicit BundledPluginIndex(std::wstring folder) : m_folder(std::move(folder)) {}

    // Empty if the folder has no bundled.json; throws PluginManagerError if it can't be read.
    std::vector<PluginListing> Fetch() override;

private:
    const std::wstring m_folder;
};

}  // namespace gazeora
