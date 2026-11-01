// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/update/interfaces/installer_runner.h"

namespace gazeora {

// Opens an MSI in Windows Installer's own wizard (msiexec /i), which asks for administrator
// permission itself.
class MsiexecRunner final : public IInstallerRunner {
public:
    void Run(const std::wstring& installerPath) override;
};

}  // namespace gazeora
