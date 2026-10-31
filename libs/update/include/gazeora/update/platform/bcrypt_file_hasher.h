// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/update/interfaces/file_hasher.h"

namespace gazeora {

// Adapter pattern: IFileHasher over Windows' CNG (BCrypt) SHA-256.
class BcryptFileHasher final : public IFileHasher {
public:
    std::string Sha256(const std::wstring& path) const override;
};

}  // namespace gazeora
