// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <windows.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugins/plugin_services.h"

namespace gazeora {
namespace {

namespace fs = std::filesystem;

const StreamProtocol kSampleProtocol("sample-out");
const VideoCodec kSampleCodec("sample-codec");

// A plugins folder of the test's own, into which the sample plugin (tests/CMakeLists.txt) is
// copied under other names, as the Plugins page installs one.
class PluginServicesTest : public ::testing::Test {
protected:
    void SetUp() override {
        folder = fs::temp_directory_path() /
                 (L"gazeora_plugin_services_test_" + std::to_wstring(GetCurrentProcessId()));
        fs::remove_all(folder);
        fs::create_directories(folder);
    }

    void TearDown() override {
        services.reset();  // unloads the copies, so they can be deleted
        std::error_code ignored;
        fs::remove_all(folder, ignored);
    }

    void Install(const std::wstring& fileName) const {
        const fs::path sample =
            fs::path(Utf8::ToWide(GAZEORA_TEST_PLUGIN_DIR)) / L"gazeora_test_plugin.dll";
        fs::copy_file(sample, folder / fileName);
    }

    void Start() { services = std::make_unique<PluginServices>(PluginFolder(folder.wstring())); }

    fs::path folder;
    std::unique_ptr<PluginServices> services;
};

TEST_F(PluginServicesTest, AddsAPluginWhileRunning) {
    Start();
    const std::shared_ptr<const GameStoreRegistry> before = services->Stores();
    ASSERT_TRUE(services->Plugins().empty());
    EXPECT_EQ(services->GamepadDrivers(), nullptr);

    Install(L"added-sample.dll");
    ASSERT_TRUE(services->CanAdd(L"added-sample.dll"));
    const PluginStatus* status = services->Add(L"added-sample.dll");
    ASSERT_NE(status, nullptr);
    EXPECT_TRUE(status->loaded) << status->error;
    EXPECT_EQ(status->name, L"Sample plugin");
    ASSERT_EQ(services->Plugins().size(), 1u);
    EXPECT_NE(services->Stores()->Find("sample"), nullptr);
    EXPECT_NE(services->GamepadDrivers(), nullptr);
    EXPECT_TRUE(services->Protocols()->Has(kSampleProtocol));
    EXPECT_TRUE(services->Outputs()->Has(kSampleProtocol));
    EXPECT_TRUE(services->Codecs()->Has(kSampleCodec));
    EXPECT_TRUE(services->Encoders()->Available(kSampleCodec));
    EXPECT_EQ(services->UiPanels().size(), 1u);
    // What was handed out before stays as it was: new registries replace it.
    EXPECT_EQ(before->Find("sample"), nullptr);
}

TEST_F(PluginServicesTest, RemovesAPluginButKeepsItsDllLoaded) {
    Install(L"sample-copy.dll");
    Start();
    const std::shared_ptr<const GameStore> store = services->Stores()->Find("sample");
    ASSERT_NE(store, nullptr);

    const std::optional<PluginStatus> removed = services->Remove(L"SAMPLE-COPY.dll");
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(removed->name, L"Sample plugin");
    EXPECT_EQ(removed->gameStores, 1);
    EXPECT_TRUE(services->Plugins().empty());
    EXPECT_EQ(services->Stores()->Find("sample"), nullptr);
    EXPECT_NE(services->Stores()->Find("steam"), nullptr);
    EXPECT_FALSE(services->Protocols()->Has(kSampleProtocol));
    EXPECT_FALSE(services->Codecs()->Has(kSampleCodec));
    EXPECT_EQ(services->GamepadDrivers(), nullptr);
    EXPECT_EQ(services->GamepadSwitch(), nullptr);
    EXPECT_TRUE(services->UiPanels().empty());
    // Still loaded for whatever still uses it.
    EXPECT_EQ(store->AppIdFor(L"C:\\Games\\sample_game.exe"), L"s-1");
    EXPECT_NE(GetModuleHandleW((folder / L"sample-copy.dll").c_str()), nullptr);
    EXPECT_FALSE(services->Remove(L"sample-copy.dll").has_value());
}

TEST_F(PluginServicesTest, NeverLoadsAFileTwice) {
    Install(L"sample-copy.dll");
    Start();
    EXPECT_FALSE(services->CanAdd(L"sample-copy.dll"));
    EXPECT_EQ(services->Add(L"sample-copy.dll"), nullptr);
    ASSERT_TRUE(services->Remove(L"sample-copy.dll").has_value());
    // Its DLL is still loaded under that name: an update waits for a restart.
    EXPECT_FALSE(services->CanAdd(L"sample-copy.dll"));
    EXPECT_EQ(services->Add(L"sample-copy.dll"), nullptr);
    EXPECT_TRUE(services->Plugins().empty());
}

TEST_F(PluginServicesTest, KeepsCardIdsWhenAnotherPluginGoes) {
    Install(L"a-sample.dll");
    Install(L"b-sample.dll");
    Start();
    ASSERT_EQ(services->UiPanels().size(), 2u);
    const std::string kept = services->UiPanels()[1].id;
    EXPECT_NE(services->UiPanels()[0].id, kept);
    ASSERT_TRUE(services->Remove(L"a-sample.dll").has_value());
    ASSERT_EQ(services->UiPanels().size(), 1u);
    EXPECT_EQ(services->UiPanels()[0].id, kept);
}

TEST_F(PluginServicesTest, ReportsAnAddedFileThatIsNoPlugin) {
    Start();
    std::ofstream(folder / L"not-a-plugin.dll") << "not a DLL";
    const PluginStatus* status = services->Add(L"not-a-plugin.dll");
    ASSERT_NE(status, nullptr);
    EXPECT_FALSE(status->loaded);
    EXPECT_FALSE(status->error.empty());
    EXPECT_EQ(services->Plugins().size(), 1u);
}

}  // namespace
}  // namespace gazeora
