// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/send_input_mouse_mover.h"

#include <windows.h>

namespace gazeora::app {

void SendInputMouseMover::Move(int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return;
    }
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(input));
}

}  // namespace gazeora::app
