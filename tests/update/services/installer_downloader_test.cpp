// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/update/services/installer_downloader.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <filesystem>

#include "gazeora/update/exceptions/update_error.h"
#include "gazeora/update/platform/bcrypt_file_hasher.h"
#include "update/fake_http_client.h"

namespace gazeora {
namespace {

constexpr char kUrl[] = "https://example.test/Gazeora-v1.1.0-x64.msi";
constexpr char kName[] = "Gazeora-v1.1.0-x64.msi";
constexpr char kBody[] = "abcd";
constexpr char kBodySha256[] = "88d4266fd4e6338d13b845fcf289579d209c897823b9217da3e161936f031589";

// A download folder of its own, removed afterwards.
class InstallerDownloaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        http.Serve(kUrl, kBody);
        std::filesystem::remove_all(folder);
    }
    void TearDown() override { std::filesystem::remove_all(folder); }

    std::wstring Download(const InstallerAsset& asset) {
        return InstallerDownloader(http, hasher, folder.wstring()).Download(asset, nullptr);
    }

    std::filesystem::path folder = std::filesystem::temp_directory_path() /
                                   ("gazeora_update_test_" + std::to_string(GetCurrentProcessId()));
    FakeHttpClient http;
    BcryptFileHasher hasher;
};

TEST_F(InstallerDownloaderTest, KeepsAFileThatMatchesItsChecksum) {
    std::wstring path = Download(InstallerAsset(kName, kUrl, 4, kBodySha256));
    EXPECT_EQ(std::filesystem::path(path), folder / kName);
    EXPECT_EQ(std::filesystem::file_size(path), 4u);
}

TEST_F(InstallerDownloaderTest, DeletesAFileWithTheWrongChecksum) {
    EXPECT_THROW(Download(InstallerAsset(kName, kUrl, 4, std::string(64, '0'))), UpdateError);
    EXPECT_FALSE(std::filesystem::exists(folder / kName));
}

TEST_F(InstallerDownloaderTest, DeletesAFileOfTheWrongSize) {
    EXPECT_THROW(Download(InstallerAsset(kName, kUrl, 5, kBodySha256)), UpdateError);
    EXPECT_FALSE(std::filesystem::exists(folder / kName));
}

TEST_F(InstallerDownloaderTest, RefusesAFileWithoutAChecksum) {
    EXPECT_THROW(Download(InstallerAsset(kName, kUrl, 4, "")), UpdateError);
    EXPECT_TRUE(http.Requests().empty());
}

TEST_F(InstallerDownloaderTest, RefusesNamesThatLeaveTheFolder) {
    for (const char* name : {"..\\evil.msi", "sub/evil.msi", "C:evil.msi", ""}) {
        EXPECT_THROW(Download(InstallerAsset(name, kUrl, 4, kBodySha256)), UpdateError) << name;
    }
    EXPECT_TRUE(http.Requests().empty());
}

TEST(BcryptFileHasherTest, HashesAnEmptyFile) {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
                                 ("gazeora_hash_test_" + std::to_string(GetCurrentProcessId()));
    {
        std::ofstream file(path, std::ios::binary);
    }
    EXPECT_EQ(BcryptFileHasher().Sha256(path.wstring()),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    std::filesystem::remove(path);
}

}  // namespace
}  // namespace gazeora
