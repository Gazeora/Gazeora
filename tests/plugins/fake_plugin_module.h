// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <map>
#include <memory>
#include <string>

#include "gazeora/plugins/interfaces/plugin_module.h"

namespace gazeora {

// A "DLL" whose exports are functions in the test.
class FakeModule final : public IPluginModule {
public:
    explicit FakeModule(std::map<std::string, void*> symbols) : m_symbols(std::move(symbols)) {}

    void* Symbol(const char* name) const override {
        auto found = m_symbols.find(name);
        return found == m_symbols.end() ? nullptr : found->second;
    }

private:
    const std::map<std::string, void*> m_symbols;
};

}  // namespace gazeora
