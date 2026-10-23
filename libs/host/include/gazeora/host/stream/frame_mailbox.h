#pragma once

#include <windows.h>

#include <cstdint>
#include <mutex>
#include <vector>

namespace gazeora {

// One tightly packed BGRA8 picture.
struct Picture {
    std::vector<uint8_t> bgra;
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t clock = 0;  // 90 kHz capture time (live frames only)
    bool stereo = false;  // two views side by side (TextureReader::Stereo)

    bool Complete() const { return bgra.size() >= size_t(width) * height * 4; }
};

// Hands frames from the capture thread to the encode thread, newest wins: a frame the
// encoder hasn't taken yet is replaced by the next, so delay never builds up. Also holds the
// idle picture. Buffers are swapped, never copied.
class FrameMailbox {
public:
    FrameMailbox();
    FrameMailbox(const FrameMailbox&) = delete;
    FrameMailbox& operator=(const FrameMailbox&) = delete;
    ~FrameMailbox();

    // Producer side. `bgra` gets an older buffer back, for reuse.
    void PostLive(uint32_t width, uint32_t height, uint64_t clock, std::vector<uint8_t>& bgra,
                  bool stereo = false);
    void PostIdle(uint32_t width, uint32_t height, std::vector<uint8_t> bgra);
    // Wakes the consumer without a frame, e.g. to stop.
    void Wake();

    // Consumer side. Waits up to `timeoutMs` for something to arrive, then takes it: the
    // live frame into `live` (true if there was one), a new idle picture into `idle`.
    bool Take(DWORD timeoutMs, Picture& live, Picture& idle);
    // Hands the last idle picture out again on the next Take, for a new consumer.
    void RepostIdle();

private:
    // A Win32 event rather than a condition variable: MinGW's wait_for(33 ms) takes about 47,
    // which halves the idle frame rate.
    HANDLE m_ready = nullptr;
    std::mutex m_mutex;
    Picture m_live;
    bool m_livePending = false;
    Picture m_idle;
    bool m_idlePending = false;
};

}  // namespace gazeora
