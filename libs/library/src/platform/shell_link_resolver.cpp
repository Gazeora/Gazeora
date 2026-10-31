// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/library/platform/shell_link_resolver.h"

#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include "util/path_text.h"

using Microsoft::WRL::ComPtr;

namespace gazeora {

namespace {

constexpr size_t kPathChars = MAX_PATH * 2;

ComPtr<IShellLinkW> OpenLink(const std::wstring& path) {
    ComPtr<IShellLinkW> link;
    ComPtr<IPersistFile> file;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&link))) ||
        FAILED(link.As(&file)) || FAILED(file->Load(path.c_str(), STGM_READ))) {
        return nullptr;
    }
    return link;
}

}  // namespace

bool ShellLinkResolver::Resolve(const std::wstring& shortcutPath, Game& game) const {
    ComPtr<IShellLinkW> link = OpenLink(shortcutPath);
    wchar_t buffer[kPathChars] = {};
    if (!link || FAILED(link->GetPath(buffer, ARRAYSIZE(buffer), nullptr, SLGP_RAWPATH)) ||
        !buffer[0]) {
        return false;  // e.g. a shortcut to a URL or a shell folder
    }
    wchar_t expanded[kPathChars];
    ExpandEnvironmentStringsW(buffer, expanded, ARRAYSIZE(expanded));
    game.exePath = expanded;

    buffer[0] = 0;
    link->GetArguments(buffer, ARRAYSIZE(buffer));
    game.arguments = buffer;
    buffer[0] = 0;
    link->GetWorkingDirectory(buffer, ARRAYSIZE(buffer));
    game.workingDir = buffer;
    if (game.name.empty()) {
        game.name = PathText::Stem(shortcutPath);
    }
    return true;
}

}  // namespace gazeora
