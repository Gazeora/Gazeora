// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "platform/winhttp_request.h"

#include "gazeora/common/text/utf8.h"
#include "gazeora/update/exceptions/update_error.h"

namespace gazeora {

namespace {

constexpr wchar_t kGet[] = L"GET";
constexpr DWORD kStatusOk = 200;

// Breaks `url` into its host, port and path, keeping only HTTPS ones.
URL_COMPONENTS CrackHttpsUrl(const std::wstring& url) {
    URL_COMPONENTS parts = {sizeof(parts)};
    parts.dwHostNameLength = DWORD(-1);
    parts.dwUrlPathLength = DWORD(-1);
    parts.dwExtraInfoLength = DWORD(-1);
    if (!WinHttpCrackUrl(url.c_str(), DWORD(url.size()), 0, &parts) ||
        parts.nScheme != INTERNET_SCHEME_HTTPS) {
        throw UpdateError("Not an HTTPS address: " + Utf8::FromWide(url));
    }
    return parts;
}

}  // namespace

WinHttpRequest::WinHttpRequest(HINTERNET session, const std::string& url) {
    Open(session, Utf8::ToWide(url));
    Send();
}

void WinHttpRequest::Open(HINTERNET session, const std::wstring& url) {
    const URL_COMPONENTS parts = CrackHttpsUrl(url);
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    m_host = Utf8::FromWide(host);
    // The path and the query after it, as one.
    const std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength + parts.dwExtraInfoLength);
    m_connection = WinHttpHandle(WinHttpConnect(session, host.c_str(), parts.nPort, 0));
    if (!m_connection) {
        Fail();
    }
    m_request = WinHttpHandle(WinHttpOpenRequest(m_connection.Get(), kGet, path.c_str(), nullptr,
                                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                 WINHTTP_FLAG_SECURE));
    if (!m_request) {
        Fail();
    }
}

void WinHttpRequest::Send() {
    if (!WinHttpSendRequest(m_request.Get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(m_request.Get(), nullptr)) {
        Fail();
    }
    DWORD status = 0;
    DWORD size = sizeof(status);
    if (!WinHttpQueryHeaders(m_request.Get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                             WINHTTP_NO_HEADER_INDEX) ||
        status != kStatusOk) {
        throw UpdateError("The update server answered with error " + std::to_string(status));
    }
}

void WinHttpRequest::Fail() const {
    const DWORD error = GetLastError();
    throw UpdateError("Couldn't reach " + m_host + " (error " + std::to_string(error) +
                      "). Check this PC's internet connection.");
}

uint64_t WinHttpRequest::ContentLength() const {
    wchar_t text[32] = {};
    DWORD size = sizeof(text);
    if (!WinHttpQueryHeaders(m_request.Get(), WINHTTP_QUERY_CONTENT_LENGTH,
                             WINHTTP_HEADER_NAME_BY_INDEX, text, &size, WINHTTP_NO_HEADER_INDEX)) {
        return 0;
    }
    return std::wcstoull(text, nullptr, 10);
}

size_t WinHttpRequest::Read(char* buffer, size_t size) {
    DWORD read = 0;
    if (!WinHttpReadData(m_request.Get(), buffer, DWORD(size), &read)) {
        throw UpdateError("The connection to " + m_host + " was lost. Try again.");
    }
    return read;
}

}  // namespace gazeora
