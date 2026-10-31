// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/services/installer_downloader.h"

#include <filesystem>
#include <system_error>

#include "gazeora/common/text/utf8.h"
#include "gazeora/update/exceptions/update_error.h"

namespace gazeora {

namespace {

// A file name from the feed must stay inside the download folder.
constexpr char kUnsafeNameChars[] = "\\/:*?\"<>|";
constexpr char kParentFolder[] = "..";

bool SafeFileName(const std::string& name) {
    return !name.empty() && name.find_first_of(kUnsafeNameChars) == std::string::npos &&
           name.find(kParentFolder) == std::string::npos;
}

void RemoveQuietly(const std::wstring& path) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

}  // namespace

std::wstring InstallerDownloader::Download(const InstallerAsset& asset,
                                           const DownloadProgress& progress) const {
    if (asset.Sha256().empty()) {
        throw UpdateError("This release has no checksum for " + asset.Name() +
                          ", so it can't be installed safely");
    }
    const std::wstring path = PathFor(asset);
    try {
        m_http.Download(asset.Url(), path, progress);
        Verify(asset, path);
    } catch (...) {
        RemoveQuietly(path);
        throw;
    }
    return path;
}

std::wstring InstallerDownloader::PathFor(const InstallerAsset& asset) const {
    if (!SafeFileName(asset.Name())) {
        throw UpdateError("The update's file name isn't usable: " + asset.Name());
    }
    std::error_code error;
    std::filesystem::create_directories(m_folder, error);
    if (error) {
        throw UpdateError("Couldn't create the download folder " + Utf8::FromWide(m_folder));
    }
    return (std::filesystem::path(m_folder) / Utf8::ToWide(asset.Name())).wstring();
}

void InstallerDownloader::Verify(const InstallerAsset& asset, const std::wstring& path) const {
    std::error_code error;
    const uintmax_t size = std::filesystem::file_size(path, error);
    if (error || (asset.Size() != 0 && size != asset.Size())) {
        throw UpdateError("The download was incomplete. Try again.");
    }
    if (m_hasher.Sha256(path) != asset.Sha256()) {
        throw UpdateError("The download didn't match its published checksum, so it was deleted");
    }
}

}  // namespace gazeora
