// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/platform/winhttp_client.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include "gazeora/update/exceptions/update_error.h"
#include "platform/winhttp_handle.h"
#include "platform/winhttp_request.h"

namespace gazeora {

namespace {

constexpr int kResolveTimeoutMs = 15000;
// WinHTTP's own default. It covers all of a host's addresses, so a shorter one runs out on an
// unreachable first address before the next is tried.
constexpr int kConnectTimeoutMs = 60000;
constexpr int kSendTimeoutMs = 15000;
constexpr int kReceiveTimeoutMs = 30000;
constexpr size_t kChunkBytes = 64 * 1024;
// The release lists are a few kilobytes; anything far bigger isn't one.
constexpr size_t kMaxGetBytes = 4 * 1024 * 1024;

}  // namespace

WinHttpClient::WinHttpClient(const std::wstring& userAgent)
    : m_session(std::make_unique<WinHttpHandle>(
          WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                      WINHTTP_NO_PROXY_BYPASS, 0))) {
    if (*m_session) {
        WinHttpSetTimeouts(m_session->Get(), kResolveTimeoutMs, kConnectTimeoutMs, kSendTimeoutMs,
                           kReceiveTimeoutMs);
    }
}

WinHttpClient::~WinHttpClient() = default;

std::string WinHttpClient::Get(const std::string& url) {
    if (!*m_session) {
        throw UpdateError("Windows' web client (WinHTTP) isn't available");
    }
    WinHttpRequest request(m_session->Get(), url);
    std::string body;
    std::vector<char> chunk(kChunkBytes);
    while (size_t read = request.Read(chunk.data(), chunk.size())) {
        body.append(chunk.data(), read);
        if (body.size() > kMaxGetBytes) {
            throw UpdateError("The update server sent more than expected");
        }
    }
    return body;
}

void WinHttpClient::Download(const std::string& url, const std::wstring& path,
                             const DownloadProgress& progress) {
    if (!*m_session) {
        throw UpdateError("Windows' web client (WinHTTP) isn't available");
    }
    WinHttpRequest request(m_session->Get(), url);
    std::ofstream file(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!file) {
        throw UpdateError("Couldn't save the update to " + std::filesystem::path(path).u8string());
    }
    const uint64_t total = request.ContentLength();
    uint64_t received = 0;
    std::vector<char> chunk(kChunkBytes);
    while (size_t read = request.Read(chunk.data(), chunk.size())) {
        if (!file.write(chunk.data(), std::streamsize(read))) {
            throw UpdateError("Couldn't save the update: is the disk full?");
        }
        received += read;
        if (progress && !progress(received, total)) {
            throw UpdateError("The download was stopped");
        }
    }
}

}  // namespace gazeora
