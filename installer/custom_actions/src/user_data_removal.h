// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "installer_session.h"
#include "user_data_folder.h"

namespace gazeora::setup {

// On uninstall, deletes the user's Gazeora data if they want that. The REMOVEUSERDATA property
// decides without asking, for quiet uninstalls: 1 deletes it, 0 keeps it.
class UserDataRemoval {
public:
    UserDataRemoval(const InstallerSession& session, const UserDataFolder& folder)
        : m_session(session), m_folder(folder) {}

    void Run() const;

private:
    bool Wanted() const;

    const InstallerSession& m_session;
    const UserDataFolder& m_folder;
};

}  // namespace gazeora::setup
