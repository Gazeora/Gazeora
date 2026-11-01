// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <cstring>

#include "sample_parts.h"

namespace sample {

namespace {

constexpr char kOwnedExe[] = "sample_game.exe";
constexpr char kAppId[] = "s-1";

void GAZEORA_CALL AppIdFor(void*, const char* exePath, char* appId, size_t capacity) {
    const size_t length = std::strlen(exePath), owned = std::strlen(kOwnedExe);
    const bool ours = length >= owned && std::strcmp(exePath + length - owned, kOwnedExe) == 0;
    Copy(ours ? kAppId : "", appId, capacity);
}

int32_t GAZEORA_CALL Launch(void*, const char* appId, char* error, size_t capacity) {
    if (std::strcmp(appId, kAppId) == 0) {
        return GAZEORA_TRUE;
    }
    Copy("unknown game", error, capacity);
    return GAZEORA_FALSE;
}

const GazeoraGameStoreV1 kStore = {
    sizeof(GazeoraGameStoreV1), nullptr, "sample", "Sample store", AppIdFor, Launch};

}  // namespace

const GazeoraGameStoreV1& Store() { return kStore; }

}  // namespace sample
