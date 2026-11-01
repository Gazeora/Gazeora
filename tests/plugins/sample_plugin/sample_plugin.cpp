// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// A plugin DLL for the tests, written only against gazeora_plugin.h as a third party would.
// Built three times, with SAMPLE_PLUGIN_API_VERSION:
//  - 3 (the default): a game store, an encoder, a codec, a protocol, a game controller
//    driver and a card on the Settings page (sample_parts.h);
//  - 1: as a plugin made before version 2, with the game store and the encoder only;
//  - 99: for a version Gazeora doesn't support.
#include "sample_parts.h"

#ifndef SAMPLE_PLUGIN_API_VERSION
#define SAMPLE_PLUGIN_API_VERSION GAZEORA_PLUGIN_API_VERSION
#endif

namespace {

constexpr bool kVersion2 = SAMPLE_PLUGIN_API_VERSION >= 2u;
constexpr bool kVersion3 = SAMPLE_PLUGIN_API_VERSION >= 3u;

const GazeoraPluginInfoV1 kInfo = {sizeof(GazeoraPluginInfoV1), SAMPLE_PLUGIN_API_VERSION,
                                   kVersion2 ? "Sample plugin" : "Sample plugin (version 1)",
                                   "1.0.0"};

bool RegisterVersion2(const GazeoraPluginHostV1* host) {
    const GazeoraPluginHostV2* v2 = GazeoraPluginHostV2Of(host);
    return v2 && v2->register_codec(host->context, &sample::Codec()) == GAZEORA_TRUE &&
           v2->register_protocol(host->context, &sample::Protocol()) == GAZEORA_TRUE;
}

bool RegisterVersion3(const GazeoraPluginHostV1* host) {
    const GazeoraPluginHostV3* v3 = GazeoraPluginHostV3Of(host);
    return v3 &&
           v3->register_gamepad_driver_provider(host->context, &sample::Gamepads()) ==
               GAZEORA_TRUE &&
           v3->register_ui_panel(host->context, &sample::Panel()) == GAZEORA_TRUE;
}

}  // namespace

GAZEORA_PLUGIN_EXPORT const GazeoraPluginInfoV1* GAZEORA_CALL GazeoraPluginInfo(void) {
    return &kInfo;
}

GAZEORA_PLUGIN_EXPORT int32_t GAZEORA_CALL GazeoraPluginRegister(const GazeoraPluginHostV1* host) {
    const bool registered =
        host->register_game_store(host->context, &sample::Store()) == GAZEORA_TRUE &&
        host->register_encoder_provider(host->context, &sample::Encoders(kVersion2)) ==
            GAZEORA_TRUE &&
        (!kVersion2 || RegisterVersion2(host)) && (!kVersion3 || RegisterVersion3(host));
    return registered ? GAZEORA_TRUE : GAZEORA_FALSE;
}

// For the tests: what the sample output saw.
GAZEORA_PLUGIN_EXPORT int32_t GAZEORA_CALL SampleFramesSent(void) { return sample::FramesSent(); }
GAZEORA_PLUGIN_EXPORT const char* GAZEORA_CALL SampleLastCodec(void) { return sample::LastCodec(); }
