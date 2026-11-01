// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora {

// A file's SHA-256, to check a download against the one its release published.
class IFileHasher {
public:
    virtual ~IFileHasher() = default;

    // Lowercase hex. Throws UpdateError if the file can't be read.
    virtual std::string Sha256(const std::wstring& path) const = 0;
};

}  // namespace gazeora
