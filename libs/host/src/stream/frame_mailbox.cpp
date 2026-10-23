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
                            std::vector<uint8_t>& bgra, bool stereo) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_live.bgra.swap(bgra);
        m_live.nv12.Reset();
        m_live.width = width;
        m_live.height = height;
        m_live.clock = clock;
        m_live.stereo = stereo;
        m_livePending = true;
    }
    Wake();
}

void FrameMailbox::PostLiveTexture(uint32_t width, uint32_t height, uint64_t clock,
                                   Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12, bool stereo) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_live.nv12 = std::move(nv12);
        m_live.width = width;
        m_live.height = height;
        m_live.clock = clock;
        m_live.stereo = stereo;
        m_livePending = true;
    }
    Wake();
}

void FrameMailbox::PostRepeat(uint64_t clock) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_repeatPending = true;
        m_repeatClock = clock;
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
        live.nv12 = std::move(m_live.nv12);
        live.width = m_live.width;
        live.height = m_live.height;
        live.clock = m_live.clock;
        live.stereo = m_live.stereo;
        m_livePending = false;
    } else if (m_repeatPending && live.Complete()) {
        live.clock = m_repeatClock;
        gotLive = true;
    }
    m_repeatPending = false;
    if (m_idlePending) {
        idle = m_idle;  // a copy: the stream restarts with it (RepostIdle)
        m_idlePending = false;
    }
    return gotLive;
}

void FrameMailbox::RepostIdle() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_idlePending = !m_idle.bgra.empty();
    }
    Wake();
}

}  // namespace gazeora
