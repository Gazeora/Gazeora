#pragma once

#include <windows.h>
#include <msi.h>

#include "installer_session.h"

namespace gazeora::setup {

// Adapter pattern: InstallerSession over Windows Installer's session handle.
class MsiInstallerSession : public InstallerSession {
public:
    explicit MsiInstallerSession(MSIHANDLE session) : m_session(session) {}

    std::wstring Property(const std::wstring& name) const override;
    bool AskYesNo(const std::wstring& question) const override;
    void Log(const std::wstring& message) const override;

private:
    int ProcessMessage(INSTALLMESSAGE type, const std::wstring& text) const;

    MSIHANDLE m_session;
};

}  // namespace gazeora::setup
