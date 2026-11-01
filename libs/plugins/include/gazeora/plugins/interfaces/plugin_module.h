// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora {

// A loaded plugin DLL. Unloaded when the last reference to it goes.
class IPluginModule {
public:
    virtual ~IPluginModule() = default;

    // The exported function `name`; null if there's none.
    virtual void* Symbol(const char* name) const = 0;
};

}  // namespace gazeora
