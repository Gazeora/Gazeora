// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <cstring>

#include "sample_parts.h"

namespace sample {

namespace {

constexpr char kLayout[] = R"(<panel page="settings" title="Sample plugin" icon="plugin">
  <toggle bind="loud" label="Loud"/>
  <button action="beep">Beep</button>
</panel>)";

constexpr char kLoud[] = "loud";

bool g_loud = false;

void GAZEORA_CALL Get(void*, const char* key, char* value, size_t capacity) {
    Copy(std::strcmp(key, kLoud) == 0 ? (g_loud ? "1" : "0") : "", value, capacity);
}

void GAZEORA_CALL Set(void*, const char* key, const char* value) {
    if (std::strcmp(key, kLoud) == 0) {
        g_loud = std::strcmp(value, "1") == 0;
    }
}

void GAZEORA_CALL Action(void*, const char*) {}

const GazeoraUiPanelV3 kPanel = {sizeof(GazeoraUiPanelV3), nullptr, kLayout, Get, Set, Action};

}  // namespace

const GazeoraUiPanelV3& Panel() { return kPanel; }

}  // namespace sample
