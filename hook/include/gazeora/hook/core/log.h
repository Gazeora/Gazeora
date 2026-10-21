#pragma once

namespace gazeora::hook {

// Writes a status line to the debugger output and to SharedHeader::status, where
// the host displays it.
void SetStatus(const char* fmt, ...);

}  // namespace gazeora::hook
