#include "gazeora/library/platform/library_location.h"

#include <windows.h>
#include <shlobj.h>

namespace gazeora {

namespace {

constexpr wchar_t kOverrideVariable[] = L"GAZEORA_LIBRARY";
constexpr wchar_t kRelativePath[] = L"\\Gazeora\\library.json";

}  // namespace

std::wstring LibraryLocation::DefaultPath() {
    wchar_t overridePath[MAX_PATH];
    if (GetEnvironmentVariableW(kOverrideVariable, overridePath, MAX_PATH) > 0) {
        return overridePath;
    }
    PWSTR appData = nullptr;
    std::wstring path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        path = std::wstring(appData) + kRelativePath;
    }
    CoTaskMemFree(appData);
    return path;
}

}  // namespace gazeora
