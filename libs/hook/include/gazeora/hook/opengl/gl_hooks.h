// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// SwapBuffers / wglSwapBuffers / wglSwapLayerBuffers hooks for OpenGL games, plus
// wglDeleteContext so capture objects are freed with the game's context.
#pragma once

namespace gazeora::hook::opengl {

// Only installs when the game has opengl32.dll loaded (or the host launched it).
bool Install();

}  // namespace gazeora::hook::opengl
