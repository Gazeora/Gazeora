// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Windows shell: Explorer, the browser and the file dialogs.
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace gazeora::app {

std::wstring FolderOf(const std::wstring& path);
// Opens a folder, or the folder of a file with the file selected.
void ShowInExplorer(const std::wstring& path);
// Opens `url` in the default browser.
void OpenUrl(const std::string& url);

enum class PickKind { Games, Image, Folder };
// The paths the user picked; empty if cancelled.
std::vector<std::wstring> PickFiles(HWND owner, PickKind kind);

}  // namespace gazeora::app
