// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::app {

// Whether a game launched from Gazeora is the window in front: mouse moves sent while another
// one is would move the desktop's pointer instead.
class GameFocus {
public:
    virtual ~GameFocus() = default;
    virtual bool GameInFront() const = 0;
};

}  // namespace gazeora::app
