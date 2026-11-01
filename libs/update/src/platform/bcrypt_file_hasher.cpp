// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/platform/bcrypt_file_hasher.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include "gazeora/update/exceptions/update_error.h"

namespace gazeora {

namespace {

constexpr size_t kChunkBytes = 64 * 1024;
constexpr size_t kSha256Bytes = 32;

std::string Hex(const std::vector<unsigned char>& bytes) {
    std::string hex;
    for (unsigned char byte : bytes) {
        char pair[3];
        std::snprintf(pair, sizeof(pair), "%02x", byte);
        hex += pair;
    }
    return hex;
}

// One SHA-256 computation, closed when done.
class Sha256Hash final {
public:
    Sha256Hash() {
        if (!BCRYPT_SUCCESS(
                BCryptOpenAlgorithmProvider(&m_algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) ||
            !BCRYPT_SUCCESS(BCryptCreateHash(m_algorithm, &m_hash, nullptr, 0, nullptr, 0, 0))) {
            throw UpdateError("Windows couldn't check the download's checksum");
        }
    }
    ~Sha256Hash() {
        if (m_hash) BCryptDestroyHash(m_hash);
        if (m_algorithm) BCryptCloseAlgorithmProvider(m_algorithm, 0);
    }
    Sha256Hash(const Sha256Hash&) = delete;
    Sha256Hash& operator=(const Sha256Hash&) = delete;

    void Add(const char* data, size_t size) {
        BCryptHashData(m_hash, reinterpret_cast<PUCHAR>(const_cast<char*>(data)), ULONG(size), 0);
    }
    std::vector<unsigned char> Finish() {
        std::vector<unsigned char> digest(kSha256Bytes);
        BCryptFinishHash(m_hash, digest.data(), ULONG(digest.size()), 0);
        return digest;
    }

private:
    BCRYPT_ALG_HANDLE m_algorithm = nullptr;
    BCRYPT_HASH_HANDLE m_hash = nullptr;
};

}  // namespace

std::string BcryptFileHasher::Sha256(const std::wstring& path) const {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) {
        throw UpdateError("Couldn't read the download to check it");
    }
    Sha256Hash hash;
    std::vector<char> chunk(kChunkBytes);
    while (file.read(chunk.data(), std::streamsize(chunk.size())) || file.gcount() > 0) {
        hash.Add(chunk.data(), size_t(file.gcount()));
    }
    return Hex(hash.Finish());
}

}  // namespace gazeora
