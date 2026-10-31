// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// What a custom action needs from the running install.
#pragma once

#include <string>

namespace gazeora::setup {

class InstallerSession {
public:
    virtual ~InstallerSession() = default;

    // The property's value; empty when it isn't set.
    virtual std::wstring Property(const std::wstring& name) const = 0;
    // Asks the user; false when they say no or the install shows no UI.
    virtual bool AskYesNo(const std::wstring& question) const = 0;
    virtual void Log(const std::wstring& message) const = 0;
};

}  // namespace gazeora::setup
