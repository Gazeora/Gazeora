#pragma once

#include <windows.h>
#include <objbase.h>

namespace gazeora::input {

// COM on the current thread for as long as this lives. Create and destroy it on the same
// thread; a thread that already has COM keeps its own.
class ComScope {
public:
    ComScope() : m_initialized(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {}
    ~ComScope() {
        if (m_initialized) {
            CoUninitialize();
        }
    }

    ComScope(const ComScope&) = delete;
    ComScope& operator=(const ComScope&) = delete;

private:
    bool m_initialized;
};

}  // namespace gazeora::input
