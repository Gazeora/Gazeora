// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// Whether a plugin's codec has everything the stream needs from it.
class PluginCodecCheck final {
public:
    PluginCodecCheck() = delete;

    // What's wrong with it, starting "a codec..."; empty if nothing is.
    static std::wstring Problem(const GazeoraCodecV2* codec);

private:
    static bool HasFunctions(const GazeoraCodecV2& codec);
    static bool HasText(const GazeoraCodecV2& codec);
};

}  // namespace gazeora
