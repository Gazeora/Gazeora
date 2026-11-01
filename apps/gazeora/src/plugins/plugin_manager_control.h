// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <functional>
#include <future>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "gazeora/update/interfaces/file_hasher.h"
#include "gazeora/update/services/installer_downloader.h"
#include "plugin_manager/interfaces/plugin_index.h"
#include "plugin_manager/models/plugin_entry.h"
#include "plugins/plugin_installer.h"
#include "plugins/plugin_list_fetch.h"

namespace gazeora::app {

// The change to the plugins folder that's running, if any.
enum class PluginTask { None, Installing, Removing };

// What was changed for a plugin since Gazeora started; it applies at the next start.
enum class PluginChange { Installed, Removed };

// State pattern: the Plugins page's work. The plugins that come with Gazeora are listed at
// once; the web's plugin list loads on a thread of its own (PluginListFetch). Downloads and
// changes to the plugins folder run one at a time on another thread. Tick() picks up the
// results on the UI thread.
class PluginManagerControl {
public:
    // `installed` is how the plugins loaded at start, and outlives this. `bundled` lists the
    // plugins that come with Gazeora, `web` the plugin list.
    PluginManagerControl(const std::vector<PluginStatus>& installed, IPluginIndex& web,
                         IPluginIndex& bundled, const InstallerDownloader& downloader,
                         const IFileHasher& hasher, IPluginInstaller& installer);
    ~PluginManagerControl();
    PluginManagerControl(const PluginManagerControl&) = delete;
    PluginManagerControl& operator=(const PluginManagerControl&) = delete;

    // Fetches the plugin list, the first time only unless `again`.
    void Refresh(bool again);
    // Installs the entry: from Gazeora's own copy for a bundled plugin, otherwise the list's
    // build. An install, an update or a reinstall.
    void Install(const PluginEntry& entry);
    // Installs a DLL from disk, named after its file.
    void InstallFile(const std::wstring& path);
    void Uninstall(const PluginEntry& entry);
    void Tick();

    PluginTask Task() const { return m_task; }
    // Whether a change is running; only one runs at a time.
    bool Busy() const { return m_task != PluginTask::None; }
    // Whether the plugin list is loading, and whether it has loaded.
    bool Listing() const { return m_list.Running(); }
    bool Listed() const { return m_listed; }
    const std::vector<PluginEntry>& Entries() const { return m_entries; }
    std::optional<PluginChange> ChangeOf(const std::wstring& id) const;
    bool RestartNeeded() const { return !m_changes.empty(); }
    // Why the last change failed, UTF-8; empty if it didn't.
    const std::string& Error() const { return m_error; }
    // Why the plugin list or the bundled plugins couldn't be read; empty if they could.
    const std::string& ListError() const { return m_listError; }

private:
    // What a change brings back to the UI thread.
    struct Outcome {
        std::string error;
        std::wstring id;
        std::optional<PluginChange> change;
    };

    void Start(PluginTask task, std::function<Outcome()> work);
    Outcome InstallFrom(const PluginEntry& entry) const;
    Outcome InstallChecked(const std::wstring& source, const std::wstring& id,
                           const std::string& sha256) const;
    Outcome InstallFromDisk(const std::wstring& path) const;
    Outcome Remove(const std::wstring& id, const std::wstring& fileName) const;
    void Apply(Outcome outcome);
    void Rejoin();
    void AddInstalledSinceStart();

    const std::vector<PluginStatus>& m_installed;
    const InstallerDownloader& m_downloader;
    const IFileHasher& m_hasher;
    IPluginInstaller& m_installer;
    PluginListFetch m_list;
    bool m_listed = false;
    std::vector<PluginListing> m_bundled;
    std::string m_listError;
    PluginTask m_task = PluginTask::None;
    std::vector<PluginEntry> m_entries;
    std::map<std::wstring, PluginChange> m_changes;
    std::string m_error;
    std::future<Outcome> m_work;
};

}  // namespace gazeora::app
