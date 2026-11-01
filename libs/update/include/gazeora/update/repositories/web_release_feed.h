// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/update/interfaces/http_client.h"
#include "gazeora/update/interfaces/release_feed.h"

namespace gazeora {

// Where Gazeora's website lists its Windows releases.
inline constexpr char kWindowsReleaseFeed[] = "https://gazeora.github.io/data/releases/windows/";

// Repository pattern: the website's release feed, a folder holding index.json (with the
// latest release) and page-N.json (all of them, newest first). Read through IHttpClient.
class WebReleaseFeed final : public IReleaseFeed {
public:
    // `folderUrl` ends in "/".
    WebReleaseFeed(IHttpClient& http, std::string folderUrl)
        : m_http(http), m_folderUrl(std::move(folderUrl)) {}

    std::optional<Release> Newest(const ReleaseFilter& wanted) override;

private:
    // The newest release `wanted` accepts on page `page`, if any.
    std::optional<Release> NewestOnPage(int page, const ReleaseFilter& wanted);

    IHttpClient& m_http;
    std::string m_folderUrl;
};

}  // namespace gazeora
