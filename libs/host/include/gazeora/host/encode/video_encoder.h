// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <d3d11.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/encode/video_codec.h"

namespace gazeora {

// A low-latency H.264 / H.265 encoder taking NV12 frames, from memory or, when opened with a
// device, as textures on that device's GPU. No B-frames, so frames come out in
// the order they go in and players never have to reorder them. An interface, so the stream
// doesn't depend on how frames are encoded: Media Foundation today (CreateVideoEncoder),
// a vendor SDK or a test double tomorrow. Use one from a single thread.
class VideoEncoder {
public:
    struct Frame {
        std::string annexB;  // the access unit, NAL units with start codes
        int64_t time = 0;    // the input's sample time, in 100 ns units
        bool keyFrame = false;
    };

    struct Settings {
        VideoCodec codec = VideoCodec::H264;
        uint32_t width = 0;  // even
        uint32_t height = 0;
        uint32_t fps = 30;
        uint32_t bitrate = 0;        // bits/s
        uint32_t keyInterval = 30;   // frames between key frames
        bool preferHardware = true;  // false skips the GPU encoders
        // Set: frames come as NV12 textures on this device (EncodeTexture), straight to a GPU
        // encoder on its adapter. Open fails if there's none that takes them.
        ID3D11Device* device = nullptr;
    };

    virtual ~VideoEncoder() = default;

    // Whether this PC has an encoder for `codec`, hardware or software. Always true for
    // H.264. Cheap: it lists the registered encoders without starting any.
    static bool Available(VideoCodec codec);

    virtual bool Open(const Settings& settings, std::wstring& error) = 0;
    virtual void Close() = 0;

    virtual bool IsOpen() const = 0;
    virtual const Settings& Current() const = 0;  // what it was opened with
    // The encoder's name, e.g. "NVIDIA H.264 Encoder MFT".
    virtual const std::wstring& Name() const = 0;
    virtual bool IsHardware() const = 0;

    // Takes effect from the next frame. Ignored by encoders that can't change it live.
    virtual void SetBitrate(uint32_t bitrate) = 0;
    virtual void RequestKeyFrame() = 0;

    // Encodes one NV12 frame of the opened size. Finished frames are appended to `out`;
    // usually exactly one, but an encoder may hold a frame back until the next one.
    virtual bool Encode(const uint8_t* nv12, int64_t time, std::vector<Frame>& out,
                        std::wstring& error) = 0;
    // The same for an NV12 texture of the opened size, on the device it was opened with.
    // Encoders without texture input refuse.
    virtual bool EncodeTexture(ID3D11Texture2D* nv12, int64_t time, std::vector<Frame>& out,
                               std::wstring& error) {
        (void)nv12, (void)time, (void)out;
        error = L"this encoder takes no textures";
        return false;
    }

    // True if this encoder is open with exactly these codec, size and device.
    bool Matches(VideoCodec codec, uint32_t width, uint32_t height,
                 ID3D11Device* device = nullptr) const {
        return IsOpen() && Current().codec == codec && Current().width == width &&
               Current().height == height && Current().device == device;
    }
};

// The encoder Gazeora uses: Media Foundation, with the GPU's encoder when there is one.
// Needs COM (multithreaded) and MFStartup on the thread that uses it.
std::unique_ptr<VideoEncoder> CreateVideoEncoder();

}  // namespace gazeora
