// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

// Set by CMake (apps/gazeora/CMakeLists.txt); on when built some other way.
#ifndef GAZEORA_STREAM_OVERLAY
#define GAZEORA_STREAM_OVERLAY 1
#endif

namespace gazeora {

// What this build of Gazeora.exe offers, as picked when configuring with CMake.
class BuildFeatures {
public:
    // The stream overlay and its settings card (GAZEORA_STREAM_OVERLAY).
    static constexpr bool kStreamOverlay = GAZEORA_STREAM_OVERLAY != 0;
};

}  // namespace gazeora
