// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::app {

// Where the update check is.
enum class UpdateState {
    Idle,         // not checked yet
    Checking,     // asking the website
    UpToDate,     // this is the newest
    Available,    // a newer release can be installed
    Downloading,  // its MSI is downloading
    Installing,   // the installer has started; Gazeora closes for it
    Failed,       // the last step went wrong
};

}  // namespace gazeora::app
