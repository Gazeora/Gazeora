// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace gazeora {

// A file attached to a release: its name, where to download it, its size and its SHA-256.
class InstallerAsset final {
public:
    InstallerAsset(std::string name, std::string url, uint64_t size, std::string sha256)
        : m_name(std::move(name)),
          m_url(std::move(url)),
          m_size(size),
          m_sha256(std::move(sha256)) {}

    const std::string& Name() const { return m_name; }
    const std::string& Url() const { return m_url; }
    uint64_t Size() const { return m_size; }
    // Lowercase hex; empty if the release didn't publish one.
    const std::string& Sha256() const { return m_sha256; }

private:
    std::string m_name;
    std::string m_url;
    uint64_t m_size;
    std::string m_sha256;
};

}  // namespace gazeora
