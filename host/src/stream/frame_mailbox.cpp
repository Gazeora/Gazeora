#include "gazeora/host/stream/frame_mailbox.h"

#include <utility>

namespace gazeora {

FrameMailbox::FrameMailbox() : m_ready(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {}

FrameMailbox::~FrameMailbox() {
    if (m_ready) {
        CloseHandle(m_ready);
    }
}

void FrameMailbox::PostLive(uint32_t width, uint32_t height, uint64_t clock,
                            std::vector<uint8_t>& bgra) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_live.bgra.swap(bgra);
        m_live.width = width;
        m_live.height = height;
        m_live.clock = clock;
        m_livePending = true;
    }
    Wake();
}

void FrameMailbox::PostIdle(uint32_t width, uint32_t height, std::vector<uint8_t> bgra) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_idle.bgra = std::move(bgra);
        m_idle.width = width;
        m_idle.height = height;
        m_idlePending = true;
    }
    Wake();
}

void FrameMailbox::Wake() {
    if (m_ready) {
        SetEvent(m_ready);
    }
}

bool FrameMailbox::Take(DWORD timeoutMs, Picture& live, Picture& idle) {
    WaitForSingleObject(m_ready, timeoutMs);
    std::lock_guard<std::mutex> lock(m_mutex);
    bool gotLive = m_livePending;
    if (m_livePending) {
        live.bgra.swap(m_live.bgra);
        live.width = m_live.width;
        live.height = m_live.height;
        live.clock = m_live.clock;
        m_livePending = false;
    }
    if (m_idlePending) {
        idle = std::move(m_idle);
        m_idlePending = false;
    }
    return gotLive;
}

}  // namespace gazeora
