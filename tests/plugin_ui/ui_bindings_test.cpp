// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_ui/services/ui_bindings.h"

#include <gtest/gtest.h>

#include "gazeora/plugin_ui/models/ui_condition.h"
#include "support/fake_ui_values.h"

namespace gazeora::ui {
namespace {

TEST(UiBindingsTest, GazeorasKeysGoToGazeoraAndTheRestToThePlugin) {
    testing::FakeUiValues gazeora, plugin;
    gazeora.values["gazeora.game-running"] = "1";
    plugin.values["enabled"] = "0";
    UiBindings bindings(gazeora, plugin);
    EXPECT_EQ(bindings.Get("gazeora.game-running"), "1");
    EXPECT_EQ(bindings.Get("enabled"), "0");
    EXPECT_EQ(bindings.Get("gazeora"), "");  // not under the prefix
    bindings.Set("enabled", "1");
    EXPECT_EQ(plugin.values["enabled"], "1");
    bindings.Act("gazeora.gamepad.retry");
    bindings.Act("refresh");
    EXPECT_EQ(gazeora.actions, std::vector<std::string>{"gazeora.gamepad.retry"});
    EXPECT_EQ(plugin.actions, std::vector<std::string>{"refresh"});
}

TEST(UiConditionTest, HoldsWhileItsValueIsTrue) {
    testing::FakeUiValues values;
    EXPECT_TRUE(UiCondition().Holds(values));
    EXPECT_FALSE(UiCondition("on").Holds(values));
    EXPECT_TRUE(UiCondition("!on").Holds(values));
    for (const char* falseValue : {"", "0", "false"}) {
        values.values["on"] = falseValue;
        EXPECT_FALSE(UiCondition("on").Holds(values)) << falseValue;
    }
    for (const char* trueValue : {"1", "true", "yes", "text"}) {
        values.values["on"] = trueValue;
        EXPECT_TRUE(UiCondition("on").Holds(values)) << trueValue;
        EXPECT_FALSE(UiCondition("!on").Holds(values)) << trueValue;
    }
}

}  // namespace
}  // namespace gazeora::ui
