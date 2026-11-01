// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include "gazeora/plugins/interfaces/plugin_module.h"

namespace gazeora {

// A DLL loaded with LoadLibrary; freed with it.
class Win32PluginModule final : public IPluginModule {
public:
    explicit Win32PluginModule(HMODULE module) : m_module(module) {}
    Win32PluginModule(const Win32PluginModule&) = delete;
    Win32PluginModule& operator=(const Win32PluginModule&) = delete;
    ~Win32PluginModule() override { FreeLibrary(m_module); }

    void* Symbol(const char* name) const override;

private:
    const HMODULE m_module;
};

}  // namespace gazeora
