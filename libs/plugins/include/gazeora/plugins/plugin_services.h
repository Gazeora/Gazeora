// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <vector>

#include "gazeora/host/encode/video_codec_catalog.h"
#include "gazeora/host/encode/video_encoder_registry.h"
#include "gazeora/host/launch/game_store_registry.h"
#include "gazeora/host/stream/frame_output_registry.h"
#include "gazeora/host/stream/stream_protocol_catalog.h"
#include "gazeora/input/interfaces/gamepad_driver_factory.h"
#include "gazeora/input/interfaces/gamepad_switch.h"
#include "gazeora/plugin_ui/models/ui_panel.h"
#include "gazeora/plugins/interfaces/plugin_module_loader.h"
#include "gazeora/plugins/models/plugin_catalog.h"
#include "gazeora/plugins/models/plugin_status.h"
#include "gazeora/plugins/platform/plugin_folder.h"

namespace gazeora {

// Facade pattern: loads the plugins in a folder and builds the registries the front ends hand
// to GameLibraryServices and FrameStreamServer. Plugin stores come after the built-in ones (a
// plugin store with a built-in id replaces it); plugin encoders come before Media Foundation,
// so they're tried first. Game controllers are read only through a plugin's driver.
class PluginServices final {
public:
    // `settingsFolder` is where plugins may keep their settings: the folder Gazeora keeps its
    // own in, or empty for none. `modules` is null for the real DLL loader.
    explicit PluginServices(const PluginFolder& folder, const std::wstring& settingsFolder = L"",
                            std::shared_ptr<const IPluginModuleLoader> modules = nullptr);
    PluginServices(const PluginServices&) = delete;
    PluginServices& operator=(const PluginServices&) = delete;

    std::shared_ptr<const GameStoreRegistry> Stores() const { return m_stores; }
    std::shared_ptr<const VideoEncoderRegistry> Encoders() const { return m_encoders; }
    std::shared_ptr<const FrameOutputRegistry> Outputs() const { return m_outputs; }
    // The protocols and codecs offered: the built-in ones (RTSP and H.264), then the plugins'
    // (the bundled plugins add UDP, HTTP, USB and H.265; a plugin's with a built-in id
    // replaces it).
    std::shared_ptr<const StreamProtocolCatalog> Protocols() const { return m_protocols; }
    std::shared_ptr<const VideoCodecCatalog> Codecs() const { return m_codecs; }
    // The first game controller driver a plugin offered (the bundled Game Controller plugin's);
    // null if none did.
    std::shared_ptr<input::IGamepadDriverFactory> GamepadDrivers() const {
        return m_gamepadDrivers;
    }
    // That driver's switch; null without a driver.
    std::shared_ptr<const input::IGamepadSwitch> GamepadSwitch() const { return m_gamepadSwitch; }
    // The cards plugins add to Gazeora's pages, in load order.
    const std::vector<ui::UiPanel>& UiPanels() const { return m_uiPanels; }
    const std::vector<PluginStatus>& Plugins() const { return m_plugins; }
    const std::wstring& FolderPath() const { return m_folder; }

private:
    void Build(const PluginCatalog& catalog);
    static std::shared_ptr<const FrameOutputRegistry> BuildOutputs(const PluginCatalog& catalog);
    static std::shared_ptr<const StreamProtocolCatalog> BuildProtocols(
        const PluginCatalog& catalog);
    static std::shared_ptr<const VideoCodecCatalog> BuildCodecs(const PluginCatalog& catalog);

    const std::wstring m_folder;
    std::shared_ptr<const GameStoreRegistry> m_stores;
    std::shared_ptr<const VideoEncoderRegistry> m_encoders;
    std::shared_ptr<const FrameOutputRegistry> m_outputs;
    std::shared_ptr<const StreamProtocolCatalog> m_protocols;
    std::shared_ptr<const VideoCodecCatalog> m_codecs;
    std::shared_ptr<input::IGamepadDriverFactory> m_gamepadDrivers;
    std::shared_ptr<const input::IGamepadSwitch> m_gamepadSwitch;
    std::vector<ui::UiPanel> m_uiPanels;
    std::vector<PluginStatus> m_plugins;
};

}  // namespace gazeora
