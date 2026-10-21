#pragma once

#include <windows.h>

#include <cstddef>

namespace gazeora {

// Spreads a frame's UDP packets over about kSpreadMs, in bursts a millisecond apart of at
// least kMinPacketsPerBurst: a 20-packet frame takes 5 ms, a 150-packet key frame 19. Sent
// all at once, the packets of a key frame overflow Wi-Fi access point and phone buffers;
// spread wider, they'd cost latency. Create and use on one thread.
class PacketPacer {
public:
    static constexpr size_t kMinPacketsPerBurst = 4;
    static constexpr size_t kSpreadMs = 20;
    static constexpr LONGLONG kBurstIntervalUs = 1000;

    PacketPacer();
    PacketPacer(const PacketPacer&) = delete;
    PacketPacer& operator=(const PacketPacer&) = delete;
    ~PacketPacer();

    static size_t PacketsPerBurst(size_t packets) {
        size_t spread = (packets + kSpreadMs - 1) / kSpreadMs;
        return spread > kMinPacketsPerBurst ? spread : kMinPacketsPerBurst;
    }

    // Waits out the gap between two bursts.
    void WaitForNextBurst();

private:
    // High resolution, so bursts can be a millisecond apart; Sleep(1) often takes 15 ms.
    HANDLE m_timer = nullptr;
};

}  // namespace gazeora
