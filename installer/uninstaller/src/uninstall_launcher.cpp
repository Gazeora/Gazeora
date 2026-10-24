#include "uninstall_launcher.h"

#include <windows.h>
#include <shellapi.h>

namespace gazeora::uninstall {

namespace {

constexpr wchar_t kMsiexec[] = L"msiexec.exe";
constexpr wchar_t kUninstallOption[] = L"/x ";

}  // namespace

bool UninstallLauncher::Start(const std::wstring& productCode) const {
    const std::wstring parameters = kUninstallOption + productCode;
    // Not waited for: this exe is one of the files msiexec removes.
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, nullptr, kMsiexec, parameters.c_str(), nullptr, SW_SHOWNORMAL));
    return result > 32;  // ShellExecute's success threshold
}

}  // namespace gazeora::uninstall
