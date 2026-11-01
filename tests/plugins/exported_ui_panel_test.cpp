// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>

#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_kit/exports/exported_ui_panel.h"
#include "gazeora/plugins/services/plugin_loader.h"
#include "plugins/fake_plugin_module.h"
#include "plugins/fake_plugin_module_loader.h"
#include "support/fake_ui_values.h"

namespace gazeora {
namespace {

constexpr char kLayout[] = R"(<panel page="settings" title="Example">
  <toggle bind="enabled" label="On"/>
  <button action="refresh">Refresh</button>
</panel>)";

const GazeoraPluginInfoV1 kInfo = {sizeof(GazeoraPluginInfoV1), GAZEORA_PLUGIN_API_VERSION,
                                   "Panel plugin", "1.0"};

// What the exported panel binds to; the current test's.
std::shared_ptr<ui::testing::FakeUiValues> g_values;
// What the host told the plugin.
std::string g_settingsFolder;

class CurrentTestValues final : public ui::IUiValues {
public:
    std::string Get(const std::string& key) const override { return g_values->Get(key); }
    void Set(const std::string& key, const std::string& value) override {
        g_values->Set(key, value);
    }
    void Act(const std::string& action) override { g_values->Act(action); }
};

const GazeoraPluginInfoV1* GAZEORA_CALL Info() { return &kInfo; }

int32_t GAZEORA_CALL Register(const GazeoraPluginHostV1* host) {
    static const ExportedUiPanel panel(kLayout, std::make_shared<CurrentTestValues>());
    const GazeoraPluginHostV3* v3 = GazeoraPluginHostV3Of(host);
    g_settingsFolder = v3->settings_folder;
    return v3->register_ui_panel(host->context, &panel.Panel());
}

int32_t GAZEORA_CALL RegisterBadLayout(const GazeoraPluginHostV1* host) {
    static const ExportedUiPanel panel("<panel page=\"settings\" title=\"X\"><blink/></panel>",
                                       std::make_shared<CurrentTestValues>());
    GazeoraPluginHostV3Of(host)->register_ui_panel(host->context, &panel.Panel());
    return GAZEORA_TRUE;
}

PluginCatalog LoadWith(void* registerPlugin, const std::wstring& settingsFolder = L"") {
    FakeModuleLoader modules(std::make_shared<FakeModule>(
        std::map<std::string, void*>{{GAZEORA_PLUGIN_INFO_SYMBOL, reinterpret_cast<void*>(&Info)},
                                     {GAZEORA_PLUGIN_REGISTER_SYMBOL, registerPlugin}}));
    return PluginLoader(modules, settingsFolder).Load({L"C:\\plugins\\panel.dll"});
}

class ExportedUiPanelTest : public ::testing::Test {
protected:
    void SetUp() override { g_values = std::make_shared<ui::testing::FakeUiValues>(); }
    void TearDown() override { g_values.reset(); }
};

TEST_F(ExportedUiPanelTest, LoadsThePanelsLayout) {
    PluginCatalog catalog = LoadWith(reinterpret_cast<void*>(&Register));
    ASSERT_EQ(catalog.uiPanels.size(), 1u);
    EXPECT_EQ(catalog.plugins[0].uiPanels, 1);
    const ui::UiLayout& layout = catalog.uiPanels[0].layout;
    EXPECT_EQ(layout.page, ui::UiPage::Settings);
    EXPECT_EQ(layout.title, "Example");
    EXPECT_EQ(layout.children.size(), 2u);
}

TEST_F(ExportedUiPanelTest, CarriesValuesAndActionsBothWays) {
    PluginCatalog catalog = LoadWith(reinterpret_cast<void*>(&Register));
    ui::IUiValues& values = *catalog.uiPanels.at(0).values;
    g_values->values["enabled"] = "1";
    EXPECT_EQ(values.Get("enabled"), "1");
    EXPECT_EQ(values.Get("missing"), "");
    values.Set("enabled", "0");
    EXPECT_EQ(g_values->values["enabled"], "0");
    values.Act("refresh");
    EXPECT_EQ(g_values->actions, std::vector<std::string>{"refresh"});
}

TEST_F(ExportedUiPanelTest, HandsThePluginTheSettingsFolder) {
    LoadWith(reinterpret_cast<void*>(&Register), L"C:\\Users\\Me\\AppData\\Roaming\\Gazeora");
    EXPECT_EQ(g_settingsFolder, "C:\\Users\\Me\\AppData\\Roaming\\Gazeora");
    LoadWith(reinterpret_cast<void*>(&Register));
    EXPECT_EQ(g_settingsFolder, "");
}

TEST_F(ExportedUiPanelTest, RefusesAPanelWhoseLayoutIsWrong) {
    PluginCatalog catalog = LoadWith(reinterpret_cast<void*>(&RegisterBadLayout));
    EXPECT_TRUE(catalog.uiPanels.empty());
    EXPECT_TRUE(catalog.plugins[0].loaded);  // its other parts still load
    EXPECT_EQ(catalog.plugins[0].error,
              L"refused a panel whose layout is wrong (line 1: <blink> isn't an element a "
              L"panel can have)");
}

}  // namespace
}  // namespace gazeora
