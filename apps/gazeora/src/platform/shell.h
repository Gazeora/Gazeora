// Windows shell: Explorer and the file dialogs.
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace gazeora::app {

std::wstring FolderOf(const std::wstring& path);
// Opens a folder, or the folder of a file with the file selected.
void ShowInExplorer(const std::wstring& path);

enum class PickKind { Games, Image, Folder };
// The paths the user picked; empty if cancelled.
std::vector<std::wstring> PickFiles(HWND owner, PickKind kind);

}  // namespace gazeora::app
