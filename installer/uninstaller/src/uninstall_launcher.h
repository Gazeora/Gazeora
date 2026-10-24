#pragma once

#include <string>

namespace gazeora::uninstall {

// Hands the uninstall to Windows Installer, which asks the user first and takes it from there.
class UninstallLauncher {
public:
    // False if msiexec couldn't be started.
    bool Start(const std::wstring& productCode) const;
};

}  // namespace gazeora::uninstall
