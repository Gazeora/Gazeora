#pragma once

namespace gazeora {

// What a streamed frame shows: a captured game, or Gazeora's own window (the idle picture).
// The phone floats Gazeora's window in the room and shows games full screen.
enum class FrameSource { Game, App };

}  // namespace gazeora
