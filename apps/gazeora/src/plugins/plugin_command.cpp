// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugins/plugin_command.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cwctype>
#include <exception>
#include <string>
#include <vector>

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugins/platform/plugin_folder.h"
#include "gazeora/update/platform/bcrypt_file_hasher.h"
#include "platform/elevated_run.h"
#include "plugin_manager/services/plugin_folder_editor.h"

namespace gazeora::app {

namespace {

constexpr size_t kInstallArguments = 5;    // command, pipe, id, source, sha256
constexpr size_t kUninstallArguments = 3;  // command, pipe, file

std::vector<std::wstring> CommandLine() {
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) {
        return {};
    }
    // Without the exe itself.
    std::vector<std::wstring> words(arguments + std::min(count, 1), arguments + count);
    LocalFree(arguments);
    return words;
}

std::string Lower(const std::wstring& text) {
    std::string lower = Utf8::FromWide(text);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return lower;
}

// Does what `words` asks; throws why it couldn't.
void Apply(const std::vector<std::wstring>& words) {
    BcryptFileHasher hasher;
    PluginFolderEditor editor(PluginFolder::Installed().Path(), hasher);
    editor.CleanUp();
    if (words[0] == kInstallPluginArgument) {
        editor.Install(words[3], words[2], Lower(words[4]));
    } else {
        editor.Uninstall(words[2]);
    }
}

}  // namespace

int PluginCommand::Run() {
    const std::vector<std::wstring> words = CommandLine();
    const bool install = words.size() == kInstallArguments && words[0] == kInstallPluginArgument;
    const bool uninstall =
        words.size() == kUninstallArguments && words[0] == kUninstallPluginArgument;
    if (!install && !uninstall) {
        return -1;
    }
    try {
        Apply(words);
        return 0;
    } catch (const std::exception& e) {
        ElevatedRun::ReportToParent(words[1], Utf8::ToWide(e.what()));
        return 1;
    }
}

}  // namespace gazeora::app
