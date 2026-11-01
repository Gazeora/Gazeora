// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/library/interfaces/file_checker.h"

namespace gazeora {

// Adapter pattern: checks files through GetFileAttributesW.
class Win32FileChecker final : public IFileChecker {
public:
    bool Exists(const std::wstring& path) const override;
};

}  // namespace gazeora
