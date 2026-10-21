// SwapBuffers / wglSwapBuffers / wglSwapLayerBuffers hooks for OpenGL games, plus
// wglDeleteContext so capture objects are freed with the game's context.
#pragma once

namespace gazeora::hook::opengl {

// Only installs when the game has opengl32.dll loaded (or the host launched it).
bool Install();

}  // namespace gazeora::hook::opengl
