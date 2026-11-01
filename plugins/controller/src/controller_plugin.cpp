// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Game controllers as a bundled plugin: SDL reads them (SdlGamepadDriver), the plugin's switch
// says whether they drive Gazeora (ControllerSettings), and its card on the Controls page is
// ui/controller_panel.xml. Without it, Gazeora reads no controllers. GazeoraPluginInfo comes
// from gazeora_add_bundled_plugin.

#include <memory>

#include "controller_panel_values.h"
#include "controller_panel_xml.h"
#include "controller_settings.h"
#include "gazeora/bundled/controller/sdl_gamepad_driver_factory.h"
#include "gazeora/common/text/utf8.h"
#include "gazeora/plugin_kit/bundled/bundled_plugin.h"
#include "gazeora/plugin_kit/exports/exported_gamepad_driver_provider.h"
#include "gazeora/plugin_kit/exports/exported_ui_panel.h"

namespace {

// What the plugin hands Gazeora, made once, at the first registration: the settings folder
// comes with it.
class ControllerPlugin final {
public:
    explicit ControllerPlugin(const std::wstring& settingsFolder)
        : m_settings(std::make_shared<gazeora::ControllerSettings>(settingsFolder)),
          m_drivers("SDL", std::make_shared<gazeora::input::SdlGamepadDriverFactory>(), m_settings),
          m_panel(kControllerPanelXml,
                  std::make_shared<gazeora::ControllerPanelValues>(m_settings)) {}

    int32_t Register(const GazeoraPluginHostV1* host) const {
        const bool registered =
            gazeora::BundledPlugin::RegisterGamepadDriverProvider(host, m_drivers.Provider()) ==
                GAZEORA_TRUE &&
            gazeora::BundledPlugin::RegisterUiPanel(host, m_panel.Panel()) == GAZEORA_TRUE;
        return registered ? GAZEORA_TRUE : GAZEORA_FALSE;
    }

private:
    const std::shared_ptr<gazeora::ControllerSettings> m_settings;
    const gazeora::ExportedGamepadDriverProvider m_drivers;
    const gazeora::ExportedUiPanel m_panel;
};

}  // namespace

GAZEORA_PLUGIN_EXPORT int32_t GAZEORA_CALL GazeoraPluginRegister(const GazeoraPluginHostV1* host) {
    const GazeoraPluginHostV3* v3 = GazeoraPluginHostV3Of(host);
    if (!v3) {
        return GAZEORA_FALSE;
    }
    static const ControllerPlugin plugin(
        gazeora::Utf8::ToWide(v3->settings_folder ? v3->settings_folder : ""));
    return plugin.Register(host);
}
