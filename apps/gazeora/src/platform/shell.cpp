#include "platform/shell.h"

#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace gazeora::app {

std::wstring FolderOf(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(0, slash);
}

void ShowInExplorer(const std::wstring& path) {
    if (GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY) {
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else {
        std::wstring args = L"/select,\"" + path + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }
}

std::vector<std::wstring> PickFiles(HWND owner, PickKind kind) {
    std::vector<std::wstring> paths;
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) {
        return paths;
    }
    FILEOPENDIALOGOPTIONS options = 0;
    dialog->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST;
    if (kind == PickKind::Games) {
        // Keep .lnk files as they are, so their arguments come along.
        options |= FOS_ALLOWMULTISELECT | FOS_NODEREFERENCELINKS;
        COMDLG_FILTERSPEC filters[] = {{L"Games and shortcuts", L"*.exe;*.lnk"},
                                       {L"All files", L"*.*"}};
        dialog->SetFileTypes(ARRAYSIZE(filters), filters);
        dialog->SetTitle(L"Add games to Gazeora");
    } else if (kind == PickKind::Image) {
        COMDLG_FILTERSPEC filters[] = {
            {L"Images", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.webp"},
            {L"All files", L"*.*"}};
        dialog->SetFileTypes(ARRAYSIZE(filters), filters);
        dialog->SetTitle(L"Choose cover art");
    } else {
        options |= FOS_PICKFOLDERS;
        dialog->SetTitle(L"Choose the folder to start the game in");
    }
    dialog->SetOptions(options);
    if (FAILED(dialog->Show(owner))) {
        return paths;  // cancelled
    }

    ComPtr<IShellItemArray> items;
    DWORD count = 0;
    if (FAILED(dialog->GetResults(&items)) || FAILED(items->GetCount(&count))) {
        return paths;
    }
    for (DWORD i = 0; i < count; ++i) {
        ComPtr<IShellItem> item;
        PWSTR path = nullptr;
        if (SUCCEEDED(items->GetItemAt(i, &item)) &&
            SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
            paths.push_back(path);
            CoTaskMemFree(path);
        }
    }
    return paths;
}

}  // namespace gazeora::app
