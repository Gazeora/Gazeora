// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/update/interfaces/http_client.h"

namespace gazeora {

class WinHttpHandle;

// Adapter pattern: IHttpClient over Windows' WinHTTP, through the system's proxy settings.
// HTTPS only.
class WinHttpClient final : public IHttpClient {
public:
    // `userAgent` names the program, e.g. "Gazeora/v1.2.0".
    explicit WinHttpClient(const std::wstring& userAgent);
    ~WinHttpClient() override;

    std::string Get(const std::string& url) override;
    void Download(const std::string& url, const std::wstring& path,
                  const DownloadProgress& progress) override;

private:
    std::unique_ptr<WinHttpHandle> m_session;
};

}  // namespace gazeora
