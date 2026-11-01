// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora::app {

// Puts a change to the plugins folder into effect in the running Gazeora, where it can be. What
// it can't put into effect waits for the next start.
class IPluginActivator {
public:
    virtual ~IPluginActivator() = default;

    // The plugin in `fileName` was installed in the plugins folder: true if Gazeora has loaded
    // it, also if it then failed to load (its status says why).
    virtual bool Activate(const std::wstring& fileName) = 0;
    // The plugin in `fileName` was uninstalled: true if Gazeora no longer uses it.
    virtual bool Deactivate(const std::wstring& fileName) = 0;
};

}  // namespace gazeora::app
