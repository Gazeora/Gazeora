// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora {

// What a streamed frame shows: a captured game, or Gazeora's own window (the idle picture).
// In a VR viewer, the phone lets the viewer's hand point at Gazeora's window, not at games.
enum class FrameSource { Game, App };

}  // namespace gazeora
