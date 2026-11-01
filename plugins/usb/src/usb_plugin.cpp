// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// The USB protocol (UsbAccessoryOutput) as a bundled plugin. GazeoraPluginInfo comes from
// gazeora_add_bundled_plugin.

#include "gazeora/bundled/usb/usb_accessory_output.h"
#include "gazeora/plugin_kit/bundled/bundled_plugin.h"

GAZEORA_PLUGIN_EXPORT int32_t GAZEORA_CALL GazeoraPluginRegister(const GazeoraPluginHostV1* host) {
    return gazeora::BundledPlugin::RegisterProtocolOf<gazeora::UsbAccessoryOutput>(host);
}
