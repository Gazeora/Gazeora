// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>
#include <vector>

namespace gazeora::app {

// The picture streamed while no game is capturing: the app icon and name on the window
// background, as BGRA8. Empty if GDI fails.
std::vector<uint8_t> RenderLogoFrame(int width, int height);

}  // namespace gazeora::app
