// The custom actions Gazeora's MSI calls (setup_actions.def exports them).

#include <windows.h>
#include <msi.h>
#include <msiquery.h>

#include "disk_user_data_folder.h"
#include "msi_installer_session.h"
#include "user_data_removal.h"

namespace {

constexpr wchar_t kAppDataProperty[] = L"AppDataFolder";
constexpr wchar_t kDataFolderName[] = L"Gazeora";

}  // namespace

// After an uninstall: the current user's %APPDATA%\Gazeora, if they want it gone. Never fails
// the uninstall.
extern "C" UINT __stdcall RemoveUserData(MSIHANDLE handle) {
    using namespace gazeora::setup;
    const MsiInstallerSession session(handle);
    const std::wstring appData = session.Property(kAppDataProperty);
    if (appData.empty()) {
        return ERROR_SUCCESS;  // a path without it would be relative
    }
    const DiskUserDataFolder folder(std::filesystem::path(appData) / kDataFolderName);
    UserDataRemoval(session, folder).Run();
    return ERROR_SUCCESS;
}
