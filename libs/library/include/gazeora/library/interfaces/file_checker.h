// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// Tells whether a file is on disk, injected so tests can fake it.
class IFileChecker {
public:
    virtual ~IFileChecker() = default;

    virtual bool Exists(const std::wstring& path) const = 0;
};

}  // namespace gazeora
