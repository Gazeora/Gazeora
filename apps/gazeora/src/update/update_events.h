// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <functional>

#include "gazeora/update/models/release.h"

namespace gazeora::app {

// Observer pattern: what UpdateControl tells the launcher, on the UI thread.
struct UpdateEvents {
    // The check at start found a newer release.
    std::function<void(const Release&)> found;
    // The installer is running and needs Gazeora closed.
    std::function<void()> installerStarted;
};

}  // namespace gazeora::app
