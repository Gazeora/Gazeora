// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::app {

// Moves the PC's mouse by raw counts, as a physical mouse does.
class MouseMover {
public:
    virtual ~MouseMover() = default;
    // Positive dx to the right, positive dy down.
    virtual void Move(int dx, int dy) = 0;
};

}  // namespace gazeora::app
