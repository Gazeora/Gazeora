// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "platform/winhttp_handle.h"

namespace gazeora {

// One HTTPS GET, sent and answered with 200 OK by the time it's constructed. Throws
// UpdateError for anything else, including a URL that isn't HTTPS.
class WinHttpRequest final {
public:
    WinHttpRequest(HINTERNET session, const std::string& url);

    // The body's size, or 0 if the server didn't say.
    uint64_t ContentLength() const;
    // Up to `size` more bytes of the body into `buffer`; 0 at its end.
    size_t Read(char* buffer, size_t size);

private:
    void Open(HINTERNET session, const std::wstring& url);
    void Send();
    // Throws the UpdateError for the WinHTTP call that just failed.
    [[noreturn]] void Fail() const;

    std::string m_host;
    WinHttpHandle m_connection;
    WinHttpHandle m_request;
};

}  // namespace gazeora
