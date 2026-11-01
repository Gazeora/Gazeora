// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/plugins/interfaces/plugin_module.h"

namespace gazeora {

// Loads plugin DLLs.
class IPluginModuleLoader {
public:
    virtual ~IPluginModuleLoader() = default;

    // Null with `error` set if the DLL can't be loaded.
    virtual std::shared_ptr<const IPluginModule> Load(const std::wstring& path,
                                                      std::wstring& error) const = 0;
};

}  // namespace gazeora
