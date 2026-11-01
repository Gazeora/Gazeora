// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <gtest/gtest.h>

#include <limits>
#include <map>
#include <memory>
#include <string>

#include "gazeora/input/exceptions/input_error.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_kit/adapters/plugin_gamepad_state.h"
#include "gazeora/plugin_kit/exports/exported_gamepad_driver_provider.h"
#include "gazeora/plugins/services/plugin_loader.h"
#include "plugins/fake_plugin_module.h"
#include "plugins/fake_plugin_module_loader.h"
#include "support/gamepad_fakes.h"

namespace gazeora {
namespace {

using input::GamepadAxis;
using input::GamepadButton;
using input::GamepadState;

const GazeoraPluginInfoV1 kInfo = {sizeof(GazeoraPluginInfoV1), GAZEORA_PLUGIN_API_VERSION,
                                   "Fake controllers", "1.0"};

// The drivers the exported provider hands out: the current test's.
std::shared_ptr<input::testing::FakeGamepadDriverFactory> g_drivers;

class CurrentTestDrivers final : public input::IGamepadDriverFactory {
public:
    std::unique_ptr<input::IGamepadDriver> Create() override { return g_drivers->Create(); }
};

const GazeoraPluginInfoV1* GAZEORA_CALL Info() { return &kInfo; }

int32_t GAZEORA_CALL Register(const GazeoraPluginHostV1* host) {
    static const ExportedGamepadDriverProvider provider("Fake",
                                                        std::make_shared<CurrentTestDrivers>());
    const GazeoraPluginHostV3* v3 = GazeoraPluginHostV3Of(host);
    return v3 ? v3->register_gamepad_driver_provider(host->context, &provider.Provider())
              : GAZEORA_FALSE;
}

int32_t GAZEORA_CALL RegisterIncomplete(const GazeoraPluginHostV1* host) {
    const GazeoraGamepadDriverProviderV3 incomplete = {sizeof(GazeoraGamepadDriverProviderV3),
                                                       nullptr, "Incomplete", nullptr};
    GazeoraPluginHostV3Of(host)->register_gamepad_driver_provider(host->context, &incomplete);
    return GAZEORA_TRUE;
}

PluginCatalog LoadWith(void* registerPlugin) {
    FakeModuleLoader modules(std::make_shared<FakeModule>(
        std::map<std::string, void*>{{GAZEORA_PLUGIN_INFO_SYMBOL, reinterpret_cast<void*>(&Info)},
                                     {GAZEORA_PLUGIN_REGISTER_SYMBOL, registerPlugin}}));
    return PluginLoader(modules).Load({L"C:\\plugins\\controllers.dll"});
}

// Fake controllers exported as a plugin's driver, then loaded back as Gazeora loads one.
class ExportedGamepadDriverTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_drivers = std::make_shared<input::testing::FakeGamepadDriverFactory>(gamepads);
        catalog = LoadWith(reinterpret_cast<void*>(&Register));
        ASSERT_EQ(catalog.gamepadDrivers.size(), 1u);
    }
    void TearDown() override { g_drivers.reset(); }

    input::testing::FakeGamepads gamepads;
    PluginCatalog catalog;
};

TEST_F(ExportedGamepadDriverTest, CountsTheDriverInThePluginsStatus) {
    ASSERT_EQ(catalog.plugins.size(), 1u);
    EXPECT_TRUE(catalog.plugins[0].loaded);
    EXPECT_EQ(catalog.plugins[0].gamepadDrivers, 1);
}

TEST_F(ExportedGamepadDriverTest, ListsTheConnectedControllers) {
    gamepads.Connect(L"DualSense Wireless Controller");
    gamepads.Connect(L"");
    std::unique_ptr<input::IGamepadDriver> driver = catalog.gamepadDrivers[0]->Create();
    driver->Update();
    const std::vector<input::GamepadInfo> connected = driver->Connected();
    ASSERT_EQ(connected.size(), 2u);
    EXPECT_EQ(connected[0].name, L"DualSense Wireless Controller");
    EXPECT_EQ(connected[1].name, L"Game controller");  // a nameless one
}

TEST_F(ExportedGamepadDriverTest, CarriesButtonsAndSticks) {
    GamepadState state;
    state.SetButton(GamepadButton::South, true);
    state.SetButton(GamepadButton::DpadRight, true);
    state.SetAxis(GamepadAxis::LeftX, -0.5f);
    state.SetAxis(GamepadAxis::RightY, 1.0f);
    gamepads.Set(state);
    std::unique_ptr<input::IGamepadDriver> driver = catalog.gamepadDrivers[0]->Create();
    EXPECT_EQ(driver->State(), state);
}

TEST_F(ExportedGamepadDriverTest, SaysWhyControllersCantBeRead) {
    g_drivers->failure = L"Game controllers can't be read: no HID.";
    try {
        catalog.gamepadDrivers[0]->Create();
        FAIL() << "expected an InputError";
    } catch (const input::InputError& error) {
        EXPECT_EQ(error.Message(), L"Game controllers can't be read: no HID.");
    }
}

TEST(PluginRegistrarGamepadTest, RefusesADriverWithMissingFields) {
    PluginCatalog catalog = LoadWith(reinterpret_cast<void*>(&RegisterIncomplete));
    EXPECT_TRUE(catalog.gamepadDrivers.empty());
    ASSERT_EQ(catalog.plugins.size(), 1u);
    EXPECT_TRUE(catalog.plugins[0].loaded);
    EXPECT_NE(catalog.plugins[0].error.find(L"game controller driver"), std::wstring::npos)
        << catalog.plugins[0].error;
}

TEST(PluginGamepadStateTest, IgnoresUnknownButtonsAndBadSticks) {
    GazeoraGamepadStateV3 plugin = {};
    plugin.struct_size = sizeof(plugin);
    plugin.buttons = (1u << GAZEORA_GAMEPAD_BUTTON_NORTH) | (1u << 31);
    plugin.axes[GAZEORA_GAMEPAD_AXIS_LEFT_X] = std::numeric_limits<float>::quiet_NaN();
    plugin.axes[GAZEORA_GAMEPAD_AXIS_LEFT_Y] = 7.0f;
    const GamepadState state = PluginGamepadState::FromPlugin(plugin);
    GamepadState expected;
    expected.SetButton(GamepadButton::North, true);
    expected.SetAxis(GamepadAxis::LeftY, 1.0f);
    EXPECT_EQ(state, expected);
}

}  // namespace
}  // namespace gazeora
