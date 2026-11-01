// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <atomic>
#include <cstdint>
#include <future>
#include <optional>
#include <string>

#include "gazeora/update/interfaces/installer_runner.h"
#include "gazeora/update/services/installer_downloader.h"
#include "gazeora/update/services/update_checker.h"
#include "settings/update_settings.h"
#include "update/update_events.h"
#include "update/update_state.h"

namespace gazeora::app {

// State pattern: checking for, downloading and starting an update. The network work runs on
// its own thread; Tick() picks up the results on the UI thread.
class UpdateControl {
public:
    UpdateControl(const UpdateChecker& checker, const InstallerDownloader& downloader,
                  IInstallerRunner& runner, const UpdateSettings& settings, UpdateEvents events);
    ~UpdateControl();

    UpdateControl(const UpdateControl&) = delete;
    UpdateControl& operator=(const UpdateControl&) = delete;

    // Checks in the background, if the saved switch says to; a newer release is announced.
    void Start();
    void Check();
    // Downloads the available release's MSI, checks it, and starts it.
    void Install();
    // Repeats the step that failed.
    void Retry();
    void Tick();

    bool CheckOnStart() const { return m_checkOnStart; }
    void SetCheckOnStart(bool check);

    UpdateState State() const { return m_state; }
    bool Busy() const;
    const ReleaseVersion& Current() const { return m_checker.Current(); }
    // The newer release, once one was found.
    const std::optional<Release>& Update() const { return m_update; }
    // How much of the download is done, 0..1; below 0 while its size is unknown.
    float Progress() const;
    const std::string& Error() const { return m_error; }

private:
    void FinishCheck();
    void FinishDownload();
    void Fail(const std::string& message);

    const UpdateChecker& m_checker;
    const InstallerDownloader& m_downloader;
    IInstallerRunner& m_runner;
    const UpdateSettings& m_settings;
    UpdateEvents m_events;
    bool m_checkOnStart;

    UpdateState m_state = UpdateState::Idle;
    bool m_announce = false;  // the check at start: tell the launcher what it finds
    std::optional<Release> m_update;
    std::string m_error;

    // Read by the download thread; declared before the futures, which wait for it.
    std::atomic<bool> m_cancel{false};
    std::atomic<uint64_t> m_received{0};
    std::atomic<uint64_t> m_total{0};
    std::future<std::optional<Release>> m_check;
    std::future<std::wstring> m_download;
};

}  // namespace gazeora::app
