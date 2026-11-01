// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_manager/services/plugin_folder_editor.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugin_manager/exceptions/plugin_manager_error.h"
#include "gazeora/plugins/platform/plugin_folder.h"
#include "gazeora/update/platform/bcrypt_file_hasher.h"

namespace gazeora {
namespace {

namespace fs = std::filesystem;

class PluginFolderEditorTest : public ::testing::Test {
protected:
    void SetUp() override {
        root = fs::temp_directory_path() /
               (L"gazeora_plugin_editor_test_" + std::to_wstring(GetCurrentProcessId()));
        fs::remove_all(root);
        fs::create_directories(root);
        folder = (root / L"plugins").wstring();
    }
    void TearDown() override {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }

    std::wstring WriteSource(const std::string& name, const std::string& bytes) const {
        const fs::path path = root / name;
        std::ofstream(path, std::ios::binary) << bytes;
        return path.wstring();
    }
    int FilesEndingIn(const std::wstring& suffix) const {
        int count = 0;
        for (const auto& file : fs::directory_iterator(folder)) {
            const std::wstring name = file.path().filename().wstring();
            count += name.size() >= suffix.size() &&
                     name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        return count;
    }

    fs::path root;
    std::wstring folder;
    BcryptFileHasher hasher;
};

TEST_F(PluginFolderEditorTest, InstallsReplacesAndRemoves) {
    PluginFolderEditor editor(folder, hasher);
    const std::wstring first = WriteSource("first.dll", "first");
    editor.Install(first, L"example", hasher.Sha256(first));
    EXPECT_EQ(PluginFolder(folder).Dlls().size(), 1u);

    const std::wstring second = WriteSource("second.dll", "second build");
    editor.Install(second, L"example", hasher.Sha256(second));
    EXPECT_EQ(hasher.Sha256(folder + L"\\example.dll"), hasher.Sha256(second));
    EXPECT_EQ(FilesEndingIn(L".removed"), 0);  // nothing had it loaded

    editor.Uninstall(L"example.dll");
    EXPECT_TRUE(PluginFolder(folder).Dlls().empty());
}

TEST_F(PluginFolderEditorTest, RefusesAFileThatDoesNotMatchItsChecksum) {
    PluginFolderEditor editor(folder, hasher);
    const std::wstring source = WriteSource("a.dll", "bytes");
    EXPECT_THROW(editor.Install(source, L"a", std::string(64, '0')), PluginManagerError);
    EXPECT_TRUE(PluginFolder(folder).Dlls().empty());
    EXPECT_EQ(FilesEndingIn(L".new"), 0);
}

TEST_F(PluginFolderEditorTest, RefusesUnsafeNames) {
    PluginFolderEditor editor(folder, hasher);
    const std::wstring source = WriteSource("a.dll", "bytes");
    EXPECT_THROW(editor.Install(source, L"..\\evil", hasher.Sha256(source)), PluginManagerError);
    EXPECT_THROW(editor.Uninstall(L"..\\gazeora.dll"), PluginManagerError);
    EXPECT_THROW(editor.Uninstall(L"missing.dll"), PluginManagerError);
}

TEST_F(PluginFolderEditorTest, MovesALoadedPluginAsideUntilItIsUnloaded) {
    PluginFolderEditor editor(folder, hasher);
    const std::wstring sample =
        Utf8::ToWide(GAZEORA_TEST_PLUGIN_DIR) + L"\\gazeora_test_plugin.dll";
    editor.Install(sample, L"sample", hasher.Sha256(sample));
    HMODULE loaded = LoadLibraryW((folder + L"\\sample.dll").c_str());
    ASSERT_NE(loaded, nullptr);

    editor.Uninstall(L"sample.dll");
    EXPECT_TRUE(PluginFolder(folder).Dlls().empty());  // the next start won't load it
    EXPECT_EQ(FilesEndingIn(L".removed"), 1);          // still loaded, so still there

    FreeLibrary(loaded);
    editor.CleanUp();
    EXPECT_EQ(FilesEndingIn(L".removed"), 0);
}

}  // namespace
}  // namespace gazeora
