// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// Starts a downloaded installer, which then runs on its own.
class IInstallerRunner {
public:
    virtual ~IInstallerRunner() = default;

    // Throws UpdateError if it couldn't be started.
    virtual void Run(const std::wstring& installerPath) = 0;
};

}  // namespace gazeora
