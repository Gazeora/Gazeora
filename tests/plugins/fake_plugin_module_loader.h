// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>

#include "gazeora/plugins/interfaces/plugin_module.h"
#include "gazeora/plugins/interfaces/plugin_module_loader.h"

namespace gazeora {

// Hands out one module for every path, or fails.
class FakeModuleLoader final : public IPluginModuleLoader {
public:
    explicit FakeModuleLoader(std::shared_ptr<const IPluginModule> module)
        : m_module(std::move(module)) {}

    std::shared_ptr<const IPluginModule> Load(const std::wstring&,
                                              std::wstring& error) const override {
        if (!m_module) {
            error = L"cannot load";
        }
        return m_module;
    }

private:
    const std::shared_ptr<const IPluginModule> m_module;
};

}  // namespace gazeora
