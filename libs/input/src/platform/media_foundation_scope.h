#pragma once

#include <windows.h>
#include <mfapi.h>

namespace gazeora::input {

// Media Foundation started for as long as this lives; starts and shutdowns are counted, so
// these nest.
class MediaFoundationScope {
public:
    MediaFoundationScope() : m_started(SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {}
    ~MediaFoundationScope() {
        if (m_started) {
            MFShutdown();
        }
    }

    MediaFoundationScope(const MediaFoundationScope&) = delete;
    MediaFoundationScope& operator=(const MediaFoundationScope&) = delete;

private:
    bool m_started;
};

}  // namespace gazeora::input
