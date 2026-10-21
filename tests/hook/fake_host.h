// Stands in for Gazeora.exe in the hook tests: creates the shared block and frame
// event for this process, then opens the hook side of the channel on them.
#pragma once

#include <windows.h>

#include "gazeora/common/shared.h"

namespace gazeora::test {

class FakeHost {
public:
    // Created on first use and kept for the whole test process.
    static FakeHost& Get();

    // The host's own view of the block (the hook maps a separate view of it).
    SharedHeader* Shared() const { return m_shared; }
    HANDLE FrameEvent() const { return m_frameEvent; }

    // Zeroes the block (keeping magic and version) and clears the frame event.
    void Reset();

private:
    FakeHost();

    HANDLE m_mapping = nullptr;
    HANDLE m_frameEvent = nullptr;
    SharedHeader* m_shared = nullptr;
};

}  // namespace gazeora::test
