// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "mappers/release_json_mapper.h"

#include <cstdint>
#include <string>
#include <vector>

using nlohmann::json;

namespace gazeora {

namespace {

// GitHub writes each file's digest as "sha256:<hex>".
constexpr char kSha256Prefix[] = "sha256:";

std::string Sha256FromDigest(const std::string& digest) {
    const std::string prefix = kSha256Prefix;
    return digest.compare(0, prefix.size(), prefix) == 0 ? digest.substr(prefix.size()) : "";
}

}  // namespace

std::optional<Release> ReleaseJsonMapper::FromJson(const json& entry) const {
    if (!entry.is_object()) {
        return std::nullopt;
    }
    std::optional<ReleaseVersion> version =
        ReleaseVersion::Parse(entry.value("tag_name", std::string()));
    if (!version) {
        return std::nullopt;
    }
    std::vector<InstallerAsset> assets;
    for (const json& asset : entry.value("assets", json::array())) {
        if (asset.is_object()) {
            assets.push_back(AssetFromJson(asset));
        }
    }
    return Release(*version, entry.value("html_url", std::string()), std::move(assets));
}

InstallerAsset ReleaseJsonMapper::AssetFromJson(const json& entry) const {
    return InstallerAsset(
        entry.value("name", std::string()), entry.value("browser_download_url", std::string()),
        entry.value("size", uint64_t(0)), Sha256FromDigest(entry.value("digest", std::string())));
}

}  // namespace gazeora
