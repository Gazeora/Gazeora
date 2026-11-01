// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_manager/repositories/bundled_plugin_index.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include "plugin_manager/mappers/plugin_listing_json_mapper.h"

namespace gazeora {

namespace {

constexpr wchar_t kDll[] = L".dll";
// The bundled plugins carry no downloads, so the architecture picks nothing.
constexpr char kNoArch[] = "";

}  // namespace

std::vector<PluginListing> BundledPluginIndex::Fetch() {
    const std::filesystem::path folder(m_folder);
    std::ifstream manifest(folder / kManifestName, std::ios::binary);
    if (!manifest) {
        return {};
    }
    const std::string text((std::istreambuf_iterator<char>(manifest)),
                           std::istreambuf_iterator<char>());
    std::vector<PluginListing> listings;
    for (PluginListing& listing : PluginListingJsonMapper(kNoArch).FromIndex(text)) {
        const std::filesystem::path dll = folder / (listing.id + kDll);
        std::error_code error;
        if (std::filesystem::is_regular_file(dll, error)) {
            listing.bundledFile = dll.wstring();
            listings.push_back(std::move(listing));
        }
    }
    return listings;
}

}  // namespace gazeora
