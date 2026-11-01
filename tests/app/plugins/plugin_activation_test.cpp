// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugins/plugin_activation.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace gazeora::app {
namespace {

namespace fs = std::filesystem;

const std::wstring kFile = L"broken.dll";

// A plugins folder of the test's own. The file put in it isn't a plugin, so it loads with an
// error, which is enough to follow it through PluginServices.
class PluginActivationTest : public ::testing::Test {
protected:
    struct Change {
        std::wstring name;
        bool loaded;
        bool added;
    };

    void SetUp() override {
        folder = fs::temp_directory_path() /
                 (L"gazeora_plugin_activation_test_" + std::to_wstring(GetCurrentProcessId()));
        fs::remove_all(folder);
        fs::create_directories(folder);
    }
    void TearDown() override {
        std::error_code ignored;
        fs::remove_all(folder, ignored);
    }

    fs::path folder;
    std::vector<Change> changes;
};

TEST_F(PluginActivationTest, LoadsAnInstallAndDropsAnUninstall) {
    PluginServices plugins{PluginFolder(folder.wstring())};
    PluginActivation activation(plugins, [this](const PluginStatus& plugin, bool added) {
        changes.push_back({plugin.name, plugin.loaded, added});
    });
    std::ofstream(folder / kFile) << "not a DLL";

    EXPECT_TRUE(activation.Activate(kFile));
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].name, kFile);
    EXPECT_FALSE(changes[0].loaded);
    EXPECT_TRUE(changes[0].added);
    EXPECT_EQ(plugins.Plugins().size(), 1u);

    EXPECT_TRUE(activation.Deactivate(kFile));
    ASSERT_EQ(changes.size(), 2u);
    EXPECT_FALSE(changes[1].added);
    EXPECT_TRUE(plugins.Plugins().empty());
}

TEST_F(PluginActivationTest, LeavesWhatItCantDoForARestart) {
    std::ofstream(folder / kFile) << "not a DLL";
    PluginServices plugins{PluginFolder(folder.wstring())};  // loaded at start
    PluginActivation activation(plugins, [this](const PluginStatus& plugin, bool added) {
        changes.push_back({plugin.name, plugin.loaded, added});
    });
    EXPECT_FALSE(activation.Activate(kFile));  // an update of a plugin loaded since start
    EXPECT_FALSE(activation.Deactivate(L"never-installed.dll"));
    EXPECT_TRUE(changes.empty());
}

}  // namespace
}  // namespace gazeora::app
