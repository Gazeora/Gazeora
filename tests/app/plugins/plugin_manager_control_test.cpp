// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugins/plugin_manager_control.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "app/plugins/plugin_manager_fakes.h"
#include "gazeora/common/text/utf8.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/update/platform/bcrypt_file_hasher.h"
#include "plugin_manager/exceptions/plugin_manager_error.h"

namespace gazeora::app {
namespace {

namespace fs = std::filesystem;

class PluginManagerControlTest : public ::testing::Test {
protected:
    void SetUp() override {
        folder = fs::temp_directory_path() /
                 (L"gazeora_plugin_control_test_" + std::to_wstring(GetCurrentProcessId()));
        fs::create_directories(folder);
        const fs::path sample = folder / L"sample.bin";
        std::ofstream(sample, std::ios::binary) << kBytes;
        sha = hasher.Sha256(sample.wstring());

        PluginStatus local;
        local.path = L"C:\\Gazeora\\bin\\plugins\\Local.dll";
        local.name = L"Local";
        local.version = L"1.0.0";
        local.loaded = true;
        installed = {local};
        index.listings = {Listing(L"local", "1.1.0"), Listing(L"remote", "2.0.0")};
    }
    void TearDown() override {
        std::error_code ignored;
        fs::remove_all(folder, ignored);
    }

    PluginListing Listing(const std::wstring& id, const std::string& version) const {
        PluginListing listing;
        listing.id = id;
        listing.name = id;
        listing.version = version;
        listing.apiVersion = GAZEORA_PLUGIN_API_VERSION;
        listing.download.emplace(Utf8::FromWide(id) + ".dll", "https://x.test/p", 0, sha);
        return listing;
    }
    static void Finish(PluginManagerControl& control) {
        for (int i = 0; i < 500 && (control.Busy() || control.Listing()); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            control.Tick();
        }
        ASSERT_FALSE(control.Busy() || control.Listing());
    }
    static const PluginEntry& Entry(const PluginManagerControl& control, const std::wstring& id) {
        for (const PluginEntry& entry : control.Entries()) {
            if (entry.id == id) {
                return entry;
            }
        }
        throw std::runtime_error("no entry");
    }

    fs::path folder;
    std::string sha;
    BcryptFileHasher hasher;
    FakeIndex index;
    FakeIndex bundled;
    FakeHttp http;
    FakeInstaller installer;
    FakeActivator activator;
    std::vector<PluginStatus> installed;
};

TEST_F(PluginManagerControlTest, ListsOnceUnlessAskedAgain) {
    InstallerDownloader downloader(http, hasher, folder.wstring());
    PluginManagerControl control(installed, index, bundled, downloader, hasher, installer,
                                 activator);
    EXPECT_EQ(control.Entries().size(), 1u);  // the installed ones, before the list
    control.Refresh(false);
    Finish(control);
    control.Refresh(false);
    Finish(control);
    EXPECT_EQ(index.fetches, 1);
    EXPECT_TRUE(control.Listed());
    EXPECT_EQ(control.Entries().size(), 2u);
    EXPECT_TRUE(Entry(control, L"local").UpdateAvailable());
    control.Refresh(true);
    Finish(control);
    EXPECT_EQ(index.fetches, 2);
}

TEST_F(PluginManagerControlTest, InstallsTheListedBuild) {
    InstallerDownloader downloader(http, hasher, folder.wstring());
    PluginManagerControl control(installed, index, bundled, downloader, hasher, installer,
                                 activator);
    control.Refresh(false);
    Finish(control);
    control.Install(Entry(control, L"remote"));
    Finish(control);
    ASSERT_EQ(installer.installed.size(), 1u);
    EXPECT_EQ(installer.installed[0], L"remote");
    EXPECT_EQ(installer.hashes[0], sha);
    EXPECT_FALSE(fs::exists(installer.sources[0]));  // the download is cleaned up
    EXPECT_EQ(control.ChangeOf(L"remote"), PluginChange::Installed);
    EXPECT_TRUE(control.RestartNeeded());
    EXPECT_TRUE(control.Error().empty());
}

TEST_F(PluginManagerControlTest, PutsAnInstallIntoEffectWithoutARestart) {
    InstallerDownloader downloader(http, hasher, folder.wstring());
    activator.live = true;
    PluginManagerControl control(installed, index, bundled, downloader, hasher, installer,
                                 activator);
    control.Refresh(false);
    Finish(control);
    control.Install(Entry(control, L"remote"));
    Finish(control);
    ASSERT_EQ(activator.activated.size(), 1u);
    EXPECT_EQ(activator.activated[0], L"remote.dll");
    EXPECT_EQ(control.ChangeOf(L"remote"), std::nullopt);
    EXPECT_FALSE(control.RestartNeeded());
}

TEST_F(PluginManagerControlTest, PutsAnUninstallIntoEffectWithoutARestart) {
    InstallerDownloader downloader(http, hasher, folder.wstring());
    activator.live = true;
    PluginManagerControl control(installed, index, bundled, downloader, hasher, installer,
                                 activator);
    control.Uninstall(Entry(control, L"local"));
    Finish(control);
    ASSERT_EQ(activator.deactivated.size(), 1u);
    EXPECT_EQ(activator.deactivated[0], L"Local.dll");
    EXPECT_EQ(control.ChangeOf(L"local"), std::nullopt);
    EXPECT_FALSE(control.RestartNeeded());
}

TEST_F(PluginManagerControlTest, LeavesAFailedChangeAlone) {
    InstallerDownloader downloader(http, hasher, folder.wstring());
    activator.live = true;
    installer.refuse = true;
    PluginManagerControl control(installed, index, bundled, downloader, hasher, installer,
                                 activator);
    control.Uninstall(Entry(control, L"local"));
    Finish(control);
    EXPECT_TRUE(activator.deactivated.empty());
    EXPECT_EQ(control.Error(), "no permission");
}

TEST_F(PluginManagerControlTest, InstallsAFileFromDiskUnderItsName) {
    InstallerDownloader downloader(http, hasher, folder.wstring());
    PluginManagerControl control(installed, index, bundled, downloader, hasher, installer,
                                 activator);
    const fs::path file = folder / L"My Plugin.dll";
    std::ofstream(file, std::ios::binary) << kBytes;
    control.InstallFile(file.wstring());
    Finish(control);
    ASSERT_EQ(installer.installed.size(), 1u);
    EXPECT_EQ(installer.installed[0], L"my-plugin");
    EXPECT_EQ(installer.hashes[0], sha);
    EXPECT_EQ(Entry(control, L"my-plugin").id, L"my-plugin");  // shown before any restart
}

TEST_F(PluginManagerControlTest, UninstallsByTheInstalledFileName) {
    InstallerDownloader downloader(http, hasher, folder.wstring());
    PluginManagerControl control(installed, index, bundled, downloader, hasher, installer,
                                 activator);
    control.Uninstall(Entry(control, L"local"));
    Finish(control);
    ASSERT_EQ(installer.removed.size(), 1u);
    EXPECT_EQ(installer.removed[0], L"Local.dll");
    EXPECT_EQ(control.ChangeOf(L"local"), PluginChange::Removed);
}

TEST_F(PluginManagerControlTest, ReportsFailures) {
    InstallerDownloader downloader(http, hasher, folder.wstring());
    PluginManagerControl control(installed, index, bundled, downloader, hasher, installer,
                                 activator);
    index.fail = true;
    control.Refresh(false);
    Finish(control);
    EXPECT_EQ(control.ListError(), "offline");
    EXPECT_TRUE(control.Error().empty());
    EXPECT_FALSE(control.Listed());
    installer.refuse = true;
    control.Uninstall(Entry(control, L"local"));
    Finish(control);
    EXPECT_EQ(control.Error(), "no permission");
    EXPECT_FALSE(control.RestartNeeded());
}

TEST_F(PluginManagerControlTest, OffersTheBundledPluginsBeforeTheListLoads) {
    const fs::path dll = folder / L"gazeora-udp.dll";
    std::ofstream(dll, std::ios::binary) << kBytes;
    PluginListing udp;
    udp.id = L"gazeora-udp";
    udp.name = L"UDP Direct";
    udp.version = "1.0.0";
    udp.apiVersion = GAZEORA_PLUGIN_API_VERSION;
    udp.bundledFile = dll.wstring();
    bundled.listings = {udp};
    InstallerDownloader downloader(http, hasher, folder.wstring());
    PluginManagerControl control(installed, index, bundled, downloader, hasher, installer,
                                 activator);
    const PluginEntry& entry = Entry(control, L"gazeora-udp");
    EXPECT_TRUE(entry.Bundled());
    EXPECT_FALSE(entry.installed.has_value());  // offered, not installed
    ASSERT_TRUE(entry.Installable());
    control.Install(entry);
    Finish(control);
    ASSERT_EQ(installer.sources.size(), 1u);
    EXPECT_EQ(installer.sources[0], dll.wstring());  // Gazeora's own copy, no download
    EXPECT_EQ(installer.hashes[0], sha);
    EXPECT_TRUE(fs::exists(dll));
}

TEST_F(PluginManagerControlTest, ChangesPluginsWhileTheListLoads) {
    InstallerDownloader downloader(http, hasher, folder.wstring());
    PluginManagerControl control(installed, index, bundled, downloader, hasher, installer,
                                 activator);
    control.Refresh(false);
    control.Uninstall(Entry(control, L"local"));
    EXPECT_TRUE(control.Busy());
    Finish(control);
    EXPECT_EQ(control.ChangeOf(L"local"), PluginChange::Removed);
    EXPECT_TRUE(control.Listed());
}

}  // namespace
}  // namespace gazeora::app
