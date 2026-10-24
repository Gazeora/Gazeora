#include "user_data_removal.h"

namespace gazeora::setup {

namespace {

constexpr wchar_t kChoiceProperty[] = L"REMOVEUSERDATA";
constexpr wchar_t kRemove[] = L"1";
constexpr wchar_t kKeep[] = L"0";

}  // namespace

void UserDataRemoval::Run() const {
    if (!m_folder.Exists() || !Wanted()) {
        return;
    }
    if (!m_folder.Remove()) {
        m_session.Log(L"Gazeora: couldn't delete all of " + m_folder.Path());
    }
}

bool UserDataRemoval::Wanted() const {
    const std::wstring choice = m_session.Property(kChoiceProperty);
    if (choice == kRemove || choice == kKeep) {
        return choice == kRemove;
    }
    return m_session.AskYesNo(L"Also delete your Gazeora game library and settings?\n\n" +
                              m_folder.Path() + L"\n\nChoose No to keep them for a later install.");
}

}  // namespace gazeora::setup
