// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <mutex>
#include <vector>

namespace gazeora {

// One picture to encode: tightly packed BGRA8, or an NV12 texture on the GPU (nv12 set),
// which then takes precedence over bgra.
struct Picture {
    std::vector<uint8_t> bgra;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12;  // even size, from GpuNv12Converter
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t clock = 0;   // 90 kHz capture time (live frames only)
    bool stereo = false;  // two views side by side (TextureReader::Stereo)

    bool Complete() const {
        return width && height && (nv12 || bgra.size() >= size_t(width) * height * 4);
    }
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
    // A live frame already on the GPU. The texture must not be drawn into again until a few
    // more frames have been posted (Nv12TexturePool).
    void PostLiveTexture(uint32_t width, uint32_t height, uint64_t clock,
                         Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12, bool stereo = false);
    // The last live frame again, at `clock`: the consumer encodes the picture it already
    // holds, so nothing is read back or copied. Ignored while a new frame is waiting.
    void PostRepeat(uint64_t clock);
    void PostIdle(uint32_t width, uint32_t height, std::vector<uint8_t> bgra);
    // Wakes the consumer without a frame, e.g. to stop.
    void Wake();

    // Consumer side. Waits up to `timeoutMs` for something to arrive, then takes it: the
    // live frame into `live` (true if there was one, or a repeat of the one `live` holds), a
    // new idle picture into `idle`.
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
    bool m_repeatPending = false;
    uint64_t m_repeatClock = 0;
    Picture m_idle;
    bool m_idlePending = false;
};

}  // namespace gazeora
