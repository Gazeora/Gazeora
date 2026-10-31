#pragma once

namespace gazeora {

// What a streamed frame shows: a captured game, or Gazeora's own window (the idle picture).
// In a VR viewer, the phone lets the viewer's hand point at Gazeora's window, not at games.
enum class FrameSource { Game, App };

}  // namespace gazeora
