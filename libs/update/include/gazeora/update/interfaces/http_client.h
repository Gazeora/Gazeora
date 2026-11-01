// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace gazeora {

// Bytes received so far and the whole size (0 if unknown). Returns false to stop.
using DownloadProgress = std::function<bool(uint64_t received, uint64_t total)>;

// HTTPS requests, injected so tests can serve their own answers. Both throw UpdateError.
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    // The body of `url`.
    virtual std::string Get(const std::string& url) = 0;
    // Saves the body of `url` to `path`, reporting progress as it goes.
    virtual void Download(const std::string& url, const std::wstring& path,
                          const DownloadProgress& progress) = 0;
};

}  // namespace gazeora
