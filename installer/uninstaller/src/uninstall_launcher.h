// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora::uninstall {

// Hands the uninstall to Windows Installer, which asks the user first and takes it from there.
class UninstallLauncher {
public:
    // False if msiexec couldn't be started.
    bool Start(const std::wstring& productCode) const;
};

}  // namespace gazeora::uninstall
