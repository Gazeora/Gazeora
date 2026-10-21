#include "gazeora/host/stream/packet_pacer.h"

// Missing from older MinGW headers.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace gazeora {

PacketPacer::PacketPacer()
    : m_timer(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                     TIMER_ALL_ACCESS)) {}

PacketPacer::~PacketPacer() {
    if (m_timer) {
        CloseHandle(m_timer);
    }
}

void PacketPacer::WaitForNextBurst() {
    LARGE_INTEGER due;
    due.QuadPart = -kBurstIntervalUs * 10;  // relative, in 100 ns units
    if (m_timer && SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE)) {
        WaitForSingleObject(m_timer, INFINITE);
    } else {
        Sleep(1);
    }
}

}  // namespace gazeora
