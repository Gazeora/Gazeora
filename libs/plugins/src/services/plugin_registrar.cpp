// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "services/plugin_registrar.h"

#include "adapters/plugin_encoder_provider.h"
#include "adapters/plugin_game_store.h"
#include "adapters/plugin_gamepad_driver_factory.h"
#include "adapters/plugin_output_factory.h"
#include "adapters/plugin_ui_values.h"
#include "gazeora/plugin_kit/adapters/plugin_codec_format.h"
#include "gazeora/plugin_kit/utils/plugin_struct.h"
#include "gazeora/plugin_kit/utils/plugin_text.h"
#include "gazeora/plugin_ui/exceptions/ui_layout_error.h"
#include "gazeora/plugin_ui/parsing/ui_xml_parser.h"
#include "services/plugin_codec_check.h"
#include "services/plugin_id_rule.h"

namespace gazeora {

namespace {

// library.json's own launchVia values, which a store id can't take.
constexpr const char* kAuto = "auto";
constexpr const char* kDirect = "direct";
// The stream server serves RTSP itself; a plugin can't take its ids.
constexpr const char* kRtspTcp = "rtsp-tcp";
constexpr const char* kRtspUdp = "rtsp-udp";

std::wstring InvalidId(const wchar_t* what, const char* id, const wchar_t* rule) {
    return std::wstring(what) + L" with an invalid id (\"" + PluginText::Wide(id) + L"\"): " + rule;
}

}  // namespace

GazeoraPluginHostV3 PluginRegistrar::Host() {
    GazeoraPluginHostV3 host = {};
    GazeoraPluginHostV1& v1 = host.v2.v1;
    v1.struct_size = sizeof(host);
    v1.api_version = GAZEORA_PLUGIN_API_VERSION;
    v1.context = this;
    v1.register_game_store = OnGameStore;
    v1.register_encoder_provider = OnEncoderProvider;
    host.v2.register_codec = OnCodec;
    host.v2.register_protocol = OnProtocol;
    host.register_gamepad_driver_provider = OnGamepadDriverProvider;
    host.register_ui_panel = OnUiPanel;
    host.settings_folder = m_settingsFolder.c_str();
    return host;
}

int32_t GAZEORA_CALL PluginRegistrar::OnGameStore(void* context, const GazeoraGameStoreV1* store) {
    return static_cast<PluginRegistrar*>(context)->AddGameStore(store);
}

int32_t GAZEORA_CALL PluginRegistrar::OnEncoderProvider(void* context,
                                                        const GazeoraEncoderProviderV1* provider) {
    return static_cast<PluginRegistrar*>(context)->AddEncoderProvider(provider);
}

int32_t GAZEORA_CALL PluginRegistrar::OnCodec(void* context, const GazeoraCodecV2* codec) {
    return static_cast<PluginRegistrar*>(context)->AddCodec(codec);
}

int32_t GAZEORA_CALL PluginRegistrar::OnProtocol(void* context, const GazeoraProtocolV2* protocol) {
    return static_cast<PluginRegistrar*>(context)->AddProtocol(protocol);
}

int32_t GAZEORA_CALL PluginRegistrar::OnGamepadDriverProvider(
    void* context, const GazeoraGamepadDriverProviderV3* provider) {
    return static_cast<PluginRegistrar*>(context)->AddGamepadDriverProvider(provider);
}

int32_t GAZEORA_CALL PluginRegistrar::OnUiPanel(void* context, const GazeoraUiPanelV3* panel) {
    return static_cast<PluginRegistrar*>(context)->AddUiPanel(panel);
}

int32_t PluginRegistrar::AddGameStore(const GazeoraGameStoreV1* store) {
    if (!store || store->struct_size < sizeof(GazeoraGameStoreV1) || !store->name ||
        !store->app_id_for || !store->launch) {
        return Refuse(L"a game store with missing fields");
    }
    if (!PluginIdRule::Accepts(store->id, {kAuto, kDirect})) {
        return Refuse(InvalidId(L"a game store", store->id,
                                L"use a-z, 0-9 and '-', not \"auto\" or \"direct\""));
    }
    m_registered.gameStores.push_back(std::make_shared<PluginGameStore>(m_module, *store));
    return GAZEORA_TRUE;
}

int32_t PluginRegistrar::AddEncoderProvider(const GazeoraEncoderProviderV1* provider) {
    if (!provider || provider->struct_size < sizeof(GazeoraEncoderProviderV1) || !provider->name ||
        !provider->supports || !provider->create) {
        return Refuse(L"an encoder with missing fields");
    }
    const auto copy = PluginStruct::Copy<GazeoraEncoderProviderV2>(provider, provider->struct_size);
    m_registered.encoderProviders.push_back(
        std::make_shared<PluginEncoderProvider>(m_module, copy));
    return GAZEORA_TRUE;
}

int32_t PluginRegistrar::AddCodec(const GazeoraCodecV2* codec) {
    std::wstring problem = PluginCodecCheck::Problem(codec);
    if (!problem.empty()) {
        return Refuse(std::move(problem));
    }
    if (!PluginIdRule::Accepts(codec->id, {})) {
        return Refuse(InvalidId(L"a codec", codec->id, L"use a-z, 0-9 and '-'"));
    }
    VideoCodecInfo info{VideoCodec(PluginText::Narrow(codec->id)), PluginText::Narrow(codec->name),
                        PluginText::Narrow(codec->choice), codec->bitrate_percent,
                        std::make_shared<PluginCodecFormat>(m_module, *codec)};
    m_registered.codecs.push_back(std::move(info));
    return GAZEORA_TRUE;
}

int32_t PluginRegistrar::AddProtocol(const GazeoraProtocolV2* protocol) {
    if (!protocol || protocol->struct_size < sizeof(GazeoraProtocolV2) || !protocol->name ||
        !protocol->help || !protocol->discovery_path || !protocol->create_output) {
        return Refuse(L"a protocol with missing fields");
    }
    if (!PluginIdRule::Accepts(protocol->id, {kRtspTcp, kRtspUdp})) {
        return Refuse(
            InvalidId(L"a protocol", protocol->id, L"use a-z, 0-9 and '-', not an RTSP one"));
    }
    StreamProtocolInfo info{StreamProtocol(PluginText::Narrow(protocol->id)),
                            PluginText::Narrow(protocol->name), PluginText::Narrow(protocol->help),
                            PluginText::Narrow(protocol->discovery_path),
                            protocol->opened_by_players == GAZEORA_TRUE};
    m_registered.protocols.push_back(std::move(info));
    m_registered.outputs.push_back(std::make_shared<PluginOutputFactory>(m_module, *protocol));
    return GAZEORA_TRUE;
}

int32_t PluginRegistrar::AddGamepadDriverProvider(const GazeoraGamepadDriverProviderV3* provider) {
    if (!provider || provider->struct_size < sizeof(GazeoraGamepadDriverProviderV3) ||
        !provider->name || !provider->create) {
        return Refuse(L"a game controller driver with missing fields");
    }
    auto drivers = std::make_shared<PluginGamepadDriverFactory>(m_module, *provider);
    m_registered.gamepadDrivers.push_back(drivers);
    m_registered.gamepadSwitches.push_back(drivers);
    return GAZEORA_TRUE;
}

int32_t PluginRegistrar::AddUiPanel(const GazeoraUiPanelV3* panel) {
    if (!panel || panel->struct_size < sizeof(GazeoraUiPanelV3) || !panel->xml || !panel->get ||
        !panel->set || !panel->action) {
        return Refuse(L"a panel with missing fields");
    }
    try {
        m_registered.uiPanels.push_back({std::string(), ui::UiXmlParser::Parse(panel->xml),
                                         std::make_shared<PluginUiValues>(m_module, *panel)});
    } catch (const ui::UiLayoutError& error) {
        return Refuse(L"a panel whose layout is wrong (" + Utf8::ToWide(error.what()) + L")");
    }
    return GAZEORA_TRUE;
}

int32_t PluginRegistrar::Refuse(std::wstring why) {
    m_refusals.push_back(L"refused " + std::move(why));
    return GAZEORA_FALSE;
}

}  // namespace gazeora
