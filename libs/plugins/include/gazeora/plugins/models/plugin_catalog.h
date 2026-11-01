// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <vector>

#include "gazeora/host/encode/video_codec_info.h"
#include "gazeora/host/encode/video_encoder_provider.h"
#include "gazeora/host/launch/game_store.h"
#include "gazeora/host/stream/frame_output_factory.h"
#include "gazeora/host/stream/stream_protocol_info.h"
#include "gazeora/input/interfaces/gamepad_driver_factory.h"
#include "gazeora/input/interfaces/gamepad_switch.h"
#include "gazeora/plugin_ui/models/ui_panel.h"
#include "gazeora/plugins/models/plugin_status.h"

namespace gazeora {

// What the plugins offered, in load order, and how each one loaded. The stores and providers
// keep their plugin's DLL loaded.
struct PluginCatalog {
    std::vector<std::shared_ptr<const GameStore>> gameStores;
    std::vector<std::shared_ptr<const VideoEncoderProvider>> encoderProviders;
    std::vector<VideoCodecInfo> codecs;  // each with its format
    // A protocol's description and its outputs' factory, in the same order.
    std::vector<StreamProtocolInfo> protocols;
    std::vector<std::shared_ptr<const FrameOutputFactory>> outputs;
    std::vector<std::shared_ptr<input::IGamepadDriverFactory>> gamepadDrivers;
    // Each driver's switch, in the same order.
    std::vector<std::shared_ptr<const input::IGamepadSwitch>> gamepadSwitches;
    std::vector<ui::UiPanel> uiPanels;  // their ids not yet set
    std::vector<PluginStatus> plugins;
};

}  // namespace gazeora
