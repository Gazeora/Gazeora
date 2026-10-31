// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "head/mouse_mover.h"

namespace gazeora::app {

// Adapter pattern: Windows' SendInput as a MouseMover. Games reading raw input see the counts
// as they are, without Windows' pointer acceleration.
class SendInputMouseMover final : public MouseMover {
public:
    void Move(int dx, int dy) override;
};

}  // namespace gazeora::app
