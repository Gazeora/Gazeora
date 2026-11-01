// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// The parts of the sample plugin, each a struct of the plugin interface. Written only against
// gazeora_plugin.h, as a third party would.
#pragma once

#include <cstdio>

#include "gazeora/plugin_api/gazeora_plugin.h"

namespace sample {

constexpr char kCodecId[] = "sample-codec";

inline void Copy(const char* text, char* buffer, size_t capacity) {
    std::snprintf(buffer, capacity, "%s", text);
}

// "sample": owns any exe named sample_game.exe, as app "s-1".
const GazeoraGameStoreV1& Store();
// An encoder of H.264 and the sample codec that turns every frame into the same tiny access
// unit. `withIds`: as a version 2 provider, which also encodes the sample codec.
const GazeoraEncoderProviderV1& Encoders(bool withIds);
// "sample-codec": H.264's NAL unit rules under another id.
const GazeoraCodecV2& Codec();
// "sample-out": an output that takes no players but counts the frames it's sent.
const GazeoraProtocolV2& Protocol();

// What the output saw, for the tests.
int FramesSent();
const char* LastCodec();

}  // namespace sample
