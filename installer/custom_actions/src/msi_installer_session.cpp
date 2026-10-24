#include "msi_installer_session.h"

#include <msiquery.h>

namespace gazeora::setup {

std::wstring MsiInstallerSession::Property(const std::wstring& name) const {
    DWORD length = 0;
    wchar_t empty[1] = L"";
    if (MsiGetPropertyW(m_session, name.c_str(), empty, &length) != ERROR_MORE_DATA) {
        return L"";
    }
    std::wstring value(++length, L'\0');  // room for the terminator
    if (MsiGetPropertyW(m_session, name.c_str(), value.data(), &length) != ERROR_SUCCESS) {
        return L"";
    }
    value.resize(length);
    return value;
}

bool MsiInstallerSession::AskYesNo(const std::wstring& question) const {
    const auto type = INSTALLMESSAGE(INSTALLMESSAGE_USER | MB_YESNO | MB_ICONQUESTION);
    return ProcessMessage(type, question) == IDYES;
}

void MsiInstallerSession::Log(const std::wstring& message) const {
    ProcessMessage(INSTALLMESSAGE_INFO, message);
}

int MsiInstallerSession::ProcessMessage(INSTALLMESSAGE type, const std::wstring& text) const {
    PMSIHANDLE record = MsiCreateRecord(0);
    MsiRecordSetStringW(record, 0, text.c_str());
    return MsiProcessMessage(m_session, type, record);
}

}  // namespace gazeora::setup
