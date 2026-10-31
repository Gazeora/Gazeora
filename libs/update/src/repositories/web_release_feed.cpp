// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/repositories/web_release_feed.h"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "gazeora/update/exceptions/update_error.h"
#include "mappers/release_json_mapper.h"

using nlohmann::json;

namespace gazeora {

namespace {

constexpr char kIndexFile[] = "index.json";
constexpr char kPagePrefix[] = "page-";
constexpr char kPageSuffix[] = ".json";
// A bad index can't make it read pages forever.
constexpr int kMaxPages = 50;

json ParseFeed(const std::string& text) {
    json feed = json::parse(text, nullptr, false);
    if (!feed.is_object()) {
        throw UpdateError("The list of Gazeora releases couldn't be read");
    }
    return feed;
}

}  // namespace

std::optional<Release> WebReleaseFeed::Newest(const ReleaseFilter& wanted) {
    const json index = ParseFeed(m_http.Get(m_folderUrl + kIndexFile));
    std::optional<Release> latest = ReleaseJsonMapper().FromJson(index.value("latest", json()));
    if (latest && wanted(*latest)) {
        return latest;
    }
    const int pages = std::min(index.value("pages", 0), kMaxPages);
    for (int page = 1; page <= pages; ++page) {
        if (std::optional<Release> found = NewestOnPage(page, wanted)) {
            return found;
        }
    }
    return std::nullopt;
}

std::optional<Release> WebReleaseFeed::NewestOnPage(int page, const ReleaseFilter& wanted) {
    const std::string url = m_folderUrl + kPagePrefix + std::to_string(page) + kPageSuffix;
    const json feed = ParseFeed(m_http.Get(url));
    ReleaseJsonMapper mapper;
    std::optional<Release> newest;
    for (const json& entry : feed.value("releases", json::array())) {
        std::optional<Release> release = mapper.FromJson(entry);
        if (release && wanted(*release) && (!newest || newest->Version() < release->Version())) {
            newest = std::move(release);
        }
    }
    return newest;
}

}  // namespace gazeora
