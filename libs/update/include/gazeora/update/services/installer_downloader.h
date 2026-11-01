// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/update/interfaces/file_hasher.h"
#include "gazeora/update/interfaces/http_client.h"
#include "gazeora/update/models/installer_asset.h"

namespace gazeora {

// Downloads a release's MSI into a folder and keeps it only if its size and SHA-256 match
// what the release published, so a damaged or swapped file is never run.
class InstallerDownloader final {
public:
    InstallerDownloader(IHttpClient& http, const IFileHasher& hasher, std::wstring folder)
        : m_http(http), m_hasher(hasher), m_folder(std::move(folder)) {}

    // The downloaded file's path. Throws UpdateError, leaving no file behind.
    std::wstring Download(const InstallerAsset& asset, const DownloadProgress& progress) const;

private:
    std::wstring PathFor(const InstallerAsset& asset) const;
    void Verify(const InstallerAsset& asset, const std::wstring& path) const;

    IHttpClient& m_http;
    const IFileHasher& m_hasher;
    std::wstring m_folder;
};

}  // namespace gazeora
