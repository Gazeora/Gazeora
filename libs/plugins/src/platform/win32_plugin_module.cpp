// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugins/platform/win32_plugin_module.h"

namespace gazeora {

void* Win32PluginModule::Symbol(const char* name) const {
    return reinterpret_cast<void*>(GetProcAddress(m_module, name));
}

}  // namespace gazeora
