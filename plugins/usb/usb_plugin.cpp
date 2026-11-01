// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// The USB protocol (UsbAccessoryOutput) as a bundled plugin.

#include <memory>

#include "gazeora/bundled_plugin/bundled_plugin.h"
#include "gazeora/host/stream/simple_frame_output_factory.h"
#include "gazeora/host/stream/usb_accessory_output.h"
#include "gazeora/plugin_kit/exports/exported_protocol.h"

GAZEORA_PLUGIN_EXPORT const GazeoraPluginInfoV1* GAZEORA_CALL GazeoraPluginInfo(void) {
    return gazeora::BundledPlugin::Info(GAZEORA_BUNDLED_PLUGIN_NAME);
}

GAZEORA_PLUGIN_EXPORT int32_t GAZEORA_CALL GazeoraPluginRegister(const GazeoraPluginHostV1* host) {
    using gazeora::UsbAccessoryOutput;
    static const gazeora::ExportedProtocol protocol(
        UsbAccessoryOutput::Info(),
        std::make_shared<gazeora::SimpleFrameOutputFactory<UsbAccessoryOutput>>(
            gazeora::StreamProtocol::Usb));
    return gazeora::BundledPlugin::RegisterProtocol(host, protocol.Protocol());
}
