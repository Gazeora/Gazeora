// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <vector>

namespace gazeora {

inline constexpr char kFeedFolder[] = "https://example.test/releases/windows/";

// A release as the website's feed lists it, with an MSI for each of `arches`.
inline std::string ReleaseJson(const std::string& tag, const std::vector<std::string>& arches,
                               const std::string& sha256 = "00ff") {
    std::string assets;
    for (const std::string& arch : arches) {
        const std::string name = "Gazeora-" + tag + "-" + arch + ".msi";
        assets += std::string(assets.empty() ? "" : ",") + R"({"name":")" + name +
                  R"(","size":4,"browser_download_url":"https://example.test/)" + name +
                  R"(","digest":"sha256:)" + sha256 + R"("})";
    }
    return R"({"tag_name":")" + tag + R"(","html_url":"https://example.test/tag/)" + tag +
           R"(","assets":[)" + assets + "]}";
}

// index.json, with `latest` and a count of pages.
inline std::string IndexJson(const std::string& latest, int pages) {
    return R"({"latest":)" + latest + R"(,"pages":)" + std::to_string(pages) + "}";
}

// page-N.json, with `releases` (joined JSON objects).
inline std::string PageJson(const std::vector<std::string>& releases) {
    std::string joined;
    for (const std::string& release : releases) {
        joined += (joined.empty() ? "" : ",") + release;
    }
    return R"({"releases":[)" + joined + "]}";
}

}  // namespace gazeora
