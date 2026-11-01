// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_manager/mappers/plugin_listing_json_mapper.h"

#include <cctype>

#include "gazeora/common/text/utf8.h"
#include "plugin_manager/exceptions/plugin_manager_error.h"
#include "plugin_manager/models/plugin_id.h"

namespace gazeora {

namespace {

using json = nlohmann::json;

constexpr char kHttps[] = "https://";
constexpr char kDigestPrefix[] = "sha256:";
constexpr size_t kSha256HexLength = 64;

bool StartsWith(const std::string& text, const char* prefix) { return text.rfind(prefix, 0) == 0; }

std::string Text(const json& entry, const char* key) {
    auto found = entry.find(key);
    return found != entry.end() && found->is_string() ? found->get<std::string>() : std::string();
}

// 0 if the field is missing or not a non-negative number.
uint64_t Unsigned(const json& entry, const char* key) {
    auto found = entry.find(key);
    return found != entry.end() && found->is_number_unsigned() ? found->get<uint64_t>() : 0;
}

std::wstring WideText(const json& entry, const char* key) { return Utf8::ToWide(Text(entry, key)); }

// The lowercase hex of "sha256:<hex>"; empty if it isn't that.
std::string Sha256Of(const std::string& digest) {
    if (!StartsWith(digest, kDigestPrefix)) {
        return {};
    }
    std::string hex = digest.substr(sizeof(kDigestPrefix) - 1);
    for (char& c : hex) {
        c = char(tolower(static_cast<unsigned char>(c)));
        if (!isxdigit(static_cast<unsigned char>(c))) {
            return {};
        }
    }
    return hex.size() == kSha256HexLength ? hex : std::string();
}

}  // namespace

std::vector<PluginListing> PluginListingJsonMapper::FromIndex(const std::string& text) const {
    json root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("plugins") ||
        !root["plugins"].is_array()) {
        throw PluginManagerError("The plugin list couldn't be read");
    }
    std::vector<PluginListing> listings;
    for (const json& entry : root["plugins"]) {
        if (auto listing = FromEntry(entry)) {
            listings.push_back(std::move(*listing));
        }
    }
    return listings;
}

std::optional<PluginListing> PluginListingJsonMapper::FromEntry(const json& entry) const {
    if (!entry.is_object()) {
        return std::nullopt;
    }
    PluginListing listing;
    listing.id = WideText(entry, "id");
    listing.name = WideText(entry, "name");
    listing.version = Text(entry, "version");
    if (!PluginId::IsValid(listing.id) || listing.name.empty() || listing.version.empty()) {
        return std::nullopt;
    }
    listing.summary = WideText(entry, "summary");
    listing.author = WideText(entry, "author");
    listing.license = WideText(entry, "license");
    std::string homepage = Text(entry, "homepage");
    listing.homepage = StartsWith(homepage, kHttps) ? homepage : std::string();
    listing.apiVersion = uint32_t(Unsigned(entry, "api_version"));
    listing.category = PluginCategories::Parse(Text(entry, "category"));
    listing.download = DownloadOf(entry, listing.id);
    return listing;
}

std::optional<InstallerAsset> PluginListingJsonMapper::DownloadOf(const json& entry,
                                                                  const std::wstring& id) const {
    auto assets = entry.find("assets");
    if (assets == entry.end() || !assets->is_object() || !assets->contains(m_arch)) {
        return std::nullopt;
    }
    const json& asset = (*assets)[m_arch];
    if (!asset.is_object()) {
        return std::nullopt;
    }
    std::string url = Text(asset, "browser_download_url");
    std::string sha256 = Sha256Of(Text(asset, "digest"));
    if (!StartsWith(url, kHttps) || sha256.empty()) {
        return std::nullopt;
    }
    // Saved under the plugin's id, whatever the download is called.
    return InstallerAsset(Utf8::FromWide(id) + ".dll", url, Unsigned(asset, "size"), sha256);
}

}  // namespace gazeora
