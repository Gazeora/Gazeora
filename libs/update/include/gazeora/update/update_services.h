// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/common/text/utf8.h"
#include "gazeora/update/platform/bcrypt_file_hasher.h"
#include "gazeora/update/platform/msiexec_runner.h"
#include "gazeora/update/platform/update_location.h"
#include "gazeora/update/platform/winhttp_client.h"
#include "gazeora/update/repositories/web_release_feed.h"
#include "gazeora/update/services/installer_downloader.h"
#include "gazeora/update/services/update_checker.h"

namespace gazeora {

// Facade pattern: the update check, download and install, wired to the website's release
// feed and to Windows. The launcher's composition root for updates.
class UpdateServices final {
public:
    // `current` is this build's version; `arch` its architecture (x64 or x86).
    UpdateServices(ReleaseVersion current, std::string arch)
        : m_http(L"Gazeora/" + Utf8::ToWide(current.Label())),
          m_feed(m_http, kWindowsReleaseFeed),
          m_checker(m_feed, current, std::move(arch)),
          m_downloader(m_http, m_hasher, UpdateLocation::DownloadFolder()) {}

    UpdateServices(const UpdateServices&) = delete;
    UpdateServices& operator=(const UpdateServices&) = delete;

    const UpdateChecker& Checker() const { return m_checker; }
    const InstallerDownloader& Downloader() const { return m_downloader; }
    IInstallerRunner& Runner() { return m_runner; }

private:
    // Declared before the services, which hold references to them.
    WinHttpClient m_http;
    BcryptFileHasher m_hasher;
    MsiexecRunner m_runner;
    WebReleaseFeed m_feed;
    UpdateChecker m_checker;
    InstallerDownloader m_downloader;
};

}  // namespace gazeora
