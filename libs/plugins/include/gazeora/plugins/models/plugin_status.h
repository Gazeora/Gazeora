// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// How loading one plugin DLL went, for the UI.
struct PluginStatus {
    std::wstring path;
    std::wstring name;  // the plugin's own; its file name until that's known
    std::wstring version;
    bool loaded = false;
    // Why it isn't loaded, or what it registered that was refused; empty if nothing.
    std::wstring error;
    int gameStores = 0;
    int encoders = 0;
    int codecs = 0;
    int protocols = 0;
    int gamepadDrivers = 0;
    int uiPanels = 0;
};

}  // namespace gazeora
