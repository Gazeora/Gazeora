// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugins/plugin_manager_control.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <system_error>

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugin_manager/models/plugin_id.h"
#include "gazeora/plugin_manager/services/plugin_directory.h"

namespace gazeora::app {

namespace {

constexpr wchar_t kDll[] = L".dll";

std::wstring FileNameOf(const std::wstring& path) {
    return std::filesystem::path(path).filename().wstring();
}

void RemoveQuietly(const std::wstring& path) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

}  // namespace

PluginManagerControl::PluginManagerControl(const std::vector<PluginStatus>& installed,
                                           IPluginIndex& web, IPluginIndex& bundled,
                                           const InstallerDownloader& downloader,
                                           const IFileHasher& hasher, IPluginInstaller& installer)
    : m_installed(installed),
      m_downloader(downloader),
      m_hasher(hasher),
      m_installer(installer),
      m_list(web) {
    // A file beside the exe: read at once, so the page has it from the start.
    try {
        m_bundled = bundled.Fetch();
    } catch (const std::exception& e) {
        m_listError = e.what();
    }
    Rejoin();
}

PluginManagerControl::~PluginManagerControl() {
    if (m_work.valid()) {
        m_work.wait();
    }
}

void PluginManagerControl::Refresh(bool again) {
    if (!m_list.Running() && (!m_listed || again)) {
        m_list.Start();
    }
}

void PluginManagerControl::Install(const PluginEntry& entry) {
    if (!Busy() && entry.Installable()) {
        Start(PluginTask::Installing, [this, entry] { return InstallFrom(entry); });
    }
}

void PluginManagerControl::InstallFile(const std::wstring& path) {
    if (!Busy()) {
        Start(PluginTask::Installing, [this, path] { return InstallFromDisk(path); });
    }
}

void PluginManagerControl::Uninstall(const PluginEntry& entry) {
    if (Busy()) {
        return;
    }
    // Installed since start: it's in the folder under its id.
    const std::wstring fileName =
        entry.installed ? FileNameOf(entry.installed->path) : entry.id + kDll;
    const std::wstring id = entry.id;
    Start(PluginTask::Removing, [this, id, fileName] { return Remove(id, fileName); });
}

void PluginManagerControl::Start(PluginTask task, std::function<Outcome()> work) {
    m_task = task;
    m_error.clear();
    m_work = std::async(std::launch::async, [work = std::move(work)] {
        try {
            return work();
        } catch (const std::exception& e) {
            Outcome failed;
            failed.error = e.what();
            return failed;
        }
    });
}

PluginManagerControl::Outcome PluginManagerControl::InstallFrom(const PluginEntry& entry) const {
    if (entry.Bundled()) {
        const std::wstring& file = entry.listed->bundledFile;
        return InstallChecked(file, entry.id, m_hasher.Sha256(file));
    }
    const InstallerAsset& asset = *entry.listed->download;
    const std::wstring downloaded = m_downloader.Download(asset, nullptr);
    Outcome outcome = InstallChecked(downloaded, entry.id, asset.Sha256());
    RemoveQuietly(downloaded);
    return outcome;
}

PluginManagerControl::Outcome PluginManagerControl::InstallChecked(
    const std::wstring& source, const std::wstring& id, const std::string& sha256) const {
    Outcome outcome;
    std::wstring error;
    if (m_installer.Install(source, id, sha256, error)) {
        outcome.id = id;
        outcome.change = PluginChange::Installed;
    } else {
        outcome.error = Utf8::FromWide(error);
    }
    return outcome;
}

PluginManagerControl::Outcome PluginManagerControl::InstallFromDisk(
    const std::wstring& path) const {
    const std::wstring id = PluginId::ForPickedFile(path);
    if (id.empty()) {
        Outcome outcome;
        outcome.error = "The file's name has no letters or digits to name the plugin after";
        return outcome;
    }
    return InstallChecked(path, id, m_hasher.Sha256(path));
}

PluginManagerControl::Outcome PluginManagerControl::Remove(const std::wstring& id,
                                                           const std::wstring& fileName) const {
    Outcome outcome;
    std::wstring error;
    if (m_installer.Uninstall(fileName, error)) {
        outcome.id = id;
        outcome.change = PluginChange::Removed;
    } else {
        outcome.error = Utf8::FromWide(error);
    }
    return outcome;
}

void PluginManagerControl::Tick() {
    if (m_list.Poll()) {
        m_listError = m_list.Error();
        m_listed = m_listed || m_listError.empty();
        Rejoin();
    }
    if (m_work.valid() && m_work.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        Apply(m_work.get());
        m_task = PluginTask::None;
    }
}

void PluginManagerControl::Apply(Outcome outcome) {
    m_error = std::move(outcome.error);
    if (outcome.change) {
        m_changes[outcome.id] = *outcome.change;
    }
    Rejoin();
}

void PluginManagerControl::Rejoin() {
    // The bundled plugins last: Gazeora's own copy wins over a listing with the same id.
    std::vector<PluginListing> listings = m_list.Listings();
    listings.insert(listings.end(), m_bundled.begin(), m_bundled.end());
    m_entries = PluginDirectory::Join(m_installed, listings);
    AddInstalledSinceStart();
}

void PluginManagerControl::AddInstalledSinceStart() {
    // Installed from disk since start: in neither list yet.
    for (const auto& [id, change] : m_changes) {
        bool known = false;
        for (const PluginEntry& entry : m_entries) {
            known = known || entry.id == id;
        }
        if (!known && change == PluginChange::Installed) {
            m_entries.push_back({id, std::nullopt, std::nullopt});
        }
    }
}

std::optional<PluginChange> PluginManagerControl::ChangeOf(const std::wstring& id) const {
    auto found = m_changes.find(id);
    return found == m_changes.end() ? std::nullopt : std::optional<PluginChange>(found->second);
}

}  // namespace gazeora::app
