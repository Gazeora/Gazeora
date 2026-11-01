// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/bundled_plugin/bundled_plugin.h"

namespace gazeora {

const GazeoraPluginInfoV1* BundledPlugin::Info(const char* name) {
    // One plugin per DLL, so one info.
    static const GazeoraPluginInfoV1 info = {sizeof(GazeoraPluginInfoV1),
                                             GAZEORA_PLUGIN_API_VERSION, name,
                                             GAZEORA_BUNDLED_PLUGIN_VERSION};
    return &info;
}

int32_t BundledPlugin::RegisterProtocol(const GazeoraPluginHostV1* host,
                                        const GazeoraProtocolV2& protocol) {
    const GazeoraPluginHostV2* v2 = GazeoraPluginHostV2Of(host);
    return v2 ? v2->register_protocol(v2->v1.context, &protocol) : GAZEORA_FALSE;
}

int32_t BundledPlugin::RegisterCodec(const GazeoraPluginHostV1* host, const GazeoraCodecV2& codec) {
    const GazeoraPluginHostV2* v2 = GazeoraPluginHostV2Of(host);
    return v2 ? v2->register_codec(v2->v1.context, &codec) : GAZEORA_FALSE;
}

}  // namespace gazeora
