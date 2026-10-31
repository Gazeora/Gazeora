// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "gazeora/update/exceptions/update_error.h"
#include "gazeora/update/interfaces/http_client.h"

namespace gazeora {

// A web server in memory: each URL serves the body it was given; others fail like a 404.
class FakeHttpClient final : public IHttpClient {
public:
    void Serve(const std::string& url, std::string body) { m_bodies[url] = std::move(body); }
    const std::vector<std::string>& Requests() const { return m_requests; }

    std::string Get(const std::string& url) override { return Body(url); }

    void Download(const std::string& url, const std::wstring& path,
                  const DownloadProgress& progress) override {
        const std::string body = Body(url);
        std::ofstream(std::filesystem::path(path), std::ios::binary) << body;
        if (progress && !progress(body.size(), body.size())) {
            throw UpdateError("stopped");
        }
    }

private:
    std::string Body(const std::string& url) {
        m_requests.push_back(url);
        auto found = m_bodies.find(url);
        if (found == m_bodies.end()) {
            throw UpdateError("not found: " + url);
        }
        return found->second;
    }

    std::map<std::string, std::string> m_bodies;
    std::vector<std::string> m_requests;
};

}  // namespace gazeora
