// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugins/plugin_services.h"

#include "gazeora/host/encode/built_in_video_codecs.h"
#include "gazeora/host/encode/built_in_video_encoders.h"
#include "gazeora/host/launch/built_in_game_stores.h"
#include "gazeora/host/stream/built_in_stream_protocols.h"
#include "gazeora/plugins/platform/win32_plugin_module_loader.h"
#include "gazeora/plugins/services/plugin_loader.h"

namespace gazeora {

namespace {

constexpr char kPanelIdPrefix[] = "plugin-panel-";

}  // namespace

PluginServices::PluginServices(const PluginFolder& folder, const std::wstring& settingsFolder,
                               std::shared_ptr<const IPluginModuleLoader> modules)
    : m_folder(folder.Path()) {
    if (!modules) {
        modules = std::make_shared<Win32PluginModuleLoader>();
    }
    Build(PluginLoader(*modules, settingsFolder).Load(folder.Dlls()));
}

void PluginServices::Build(const PluginCatalog& catalog) {
    auto stores = std::make_shared<GameStoreRegistry>();
    BuiltInGameStores::RegisterInto(*stores);
    for (const auto& store : catalog.gameStores) {
        stores->Register(store);
    }
    auto encoders = std::make_shared<VideoEncoderRegistry>();
    for (const auto& provider : catalog.encoderProviders) {
        encoders->Register(provider);
    }
    BuiltInVideoEncoders::RegisterInto(*encoders);
    m_stores = std::move(stores);
    m_encoders = std::move(encoders);
    m_outputs = BuildOutputs(catalog);
    m_protocols = BuildProtocols(catalog);
    m_codecs = BuildCodecs(catalog);
    if (!catalog.gamepadDrivers.empty()) {
        m_gamepadDrivers = catalog.gamepadDrivers.front();
        m_gamepadSwitch = catalog.gamepadSwitches.front();
    }
    m_uiPanels = catalog.uiPanels;
    for (size_t i = 0; i < m_uiPanels.size(); ++i) {
        m_uiPanels[i].id = kPanelIdPrefix + std::to_string(i);
    }
    m_plugins = catalog.plugins;
}

std::shared_ptr<const FrameOutputRegistry> PluginServices::BuildOutputs(
    const PluginCatalog& catalog) {
    // RTSP, the only protocol without a plugin, the stream server serves itself.
    auto outputs = std::make_shared<FrameOutputRegistry>();
    for (const auto& factory : catalog.outputs) {
        outputs->Register(factory);
    }
    return outputs;
}

std::shared_ptr<const StreamProtocolCatalog> PluginServices::BuildProtocols(
    const PluginCatalog& catalog) {
    auto protocols = std::make_shared<StreamProtocolCatalog>();
    BuiltInStreamProtocols::RegisterInto(*protocols);
    for (const StreamProtocolInfo& protocol : catalog.protocols) {
        protocols->Register(protocol);
    }
    return protocols;
}

std::shared_ptr<const VideoCodecCatalog> PluginServices::BuildCodecs(const PluginCatalog& catalog) {
    auto codecs = std::make_shared<VideoCodecCatalog>();
    BuiltInVideoCodecs::RegisterInto(*codecs);
    for (const VideoCodecInfo& codec : catalog.codecs) {
        codecs->Register(codec);
    }
    return codecs;
}

}  // namespace gazeora
