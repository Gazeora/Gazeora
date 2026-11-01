// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/plugins/interfaces/plugin_module_loader.h"

namespace gazeora {

// Loads plugin DLLs with LoadLibraryEx. A DLL's own dependencies are looked for in its folder
// and the system folders, not the current directory.
class Win32PluginModuleLoader final : public IPluginModuleLoader {
public:
    std::shared_ptr<const IPluginModule> Load(const std::wstring& path,
                                              std::wstring& error) const override;
};

}  // namespace gazeora
