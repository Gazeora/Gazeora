// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "update/update_control.h"

#include <chrono>
#include <exception>
#include <utility>

namespace gazeora::app {

namespace {

constexpr float kUnknownProgress = -1.0f;

template <typename T>
bool Ready(const std::future<T>& future) {
    return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

}  // namespace

UpdateControl::UpdateControl(const UpdateChecker& checker, const InstallerDownloader& downloader,
                             IInstallerRunner& runner, const UpdateSettings& settings,
                             UpdateEvents events)
    : m_checker(checker),
      m_downloader(downloader),
      m_runner(runner),
      m_settings(settings),
      m_events(std::move(events)),
      m_checkOnStart(settings.LoadCheckOnStart()) {}

// The futures wait for their threads; a download stops at its next chunk.
UpdateControl::~UpdateControl() { m_cancel = true; }

void UpdateControl::Start() {
    if (m_checkOnStart) {
        Check();
        m_announce = true;
    }
}

void UpdateControl::Check() {
    if (Busy()) {
        return;
    }
    m_announce = false;
    m_state = UpdateState::Checking;
    m_check = std::async(std::launch::async, [this] { return m_checker.FindUpdate(); });
}

void UpdateControl::Install() {
    if (Busy() || !m_update) {
        return;
    }
    const InstallerAsset* asset = m_update->InstallerFor(m_checker.Arch());
    if (!asset) {
        return;  // the checker only offers releases with one
    }
    m_received = 0;
    m_total = asset->Size();
    m_state = UpdateState::Downloading;
    m_download = std::async(std::launch::async, [this, installer = *asset] {
        return m_downloader.Download(installer, [this](uint64_t received, uint64_t total) {
            m_received = received;
            if (total != 0) m_total = total;
            return !m_cancel.load();
        });
    });
}

void UpdateControl::Retry() {
    if (m_update) {
        Install();
    } else {
        Check();
    }
}

void UpdateControl::Tick() {
    if (Ready(m_check)) {
        FinishCheck();
    }
    if (Ready(m_download)) {
        FinishDownload();
    }
}

void UpdateControl::FinishCheck() {
    try {
        m_update = m_check.get();
    } catch (const std::exception& e) {
        Fail(e.what());
        return;
    }
    m_state = m_update ? UpdateState::Available : UpdateState::UpToDate;
    if (m_update && m_announce && m_events.found) {
        m_events.found(*m_update);
    }
}

void UpdateControl::FinishDownload() {
    try {
        m_runner.Run(m_download.get());
    } catch (const std::exception& e) {
        Fail(e.what());
        return;
    }
    m_state = UpdateState::Installing;
    if (m_events.installerStarted) {
        m_events.installerStarted();
    }
}

void UpdateControl::Fail(const std::string& message) {
    m_error = message;
    m_state = UpdateState::Failed;
}

void UpdateControl::SetCheckOnStart(bool check) {
    m_checkOnStart = check;
    m_settings.SaveCheckOnStart(check);
}

bool UpdateControl::Busy() const {
    return m_state == UpdateState::Checking || m_state == UpdateState::Downloading;
}

float UpdateControl::Progress() const {
    const uint64_t total = m_total;
    return total == 0 ? kUnknownProgress : float(double(m_received) / double(total));
}

}  // namespace gazeora::app
