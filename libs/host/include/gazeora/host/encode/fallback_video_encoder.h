// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/encode/video_encoder.h"
#include "gazeora/host/encode/video_encoder_provider.h"

namespace gazeora {

// Chain of Responsibility pattern: each Open tries the providers in order and keeps the first
// encoder that opens for the codec. Everything else goes to that encoder.
class FallbackVideoEncoder final : public VideoEncoder {
public:
    explicit FallbackVideoEncoder(
        std::vector<std::shared_ptr<const VideoEncoderProvider>> providers);

    bool Open(const Settings& settings, std::wstring& error) override;
    void Close() override;
    bool IsOpen() const override;
    const Settings& Current() const override;
    const std::wstring& Name() const override;
    bool IsHardware() const override;
    void SetBitrate(uint32_t bitrate) override;
    void RequestKeyFrame() override;
    bool Encode(const uint8_t* nv12, int64_t time, std::vector<Frame>& out,
                std::wstring& error) override;
    bool EncodeTexture(ID3D11Texture2D* nv12, int64_t time, std::vector<Frame>& out,
                       std::wstring& error) override;

private:
    // Opens one provider's encoder; on failure adds why to `errors`.
    bool TryOpen(const VideoEncoderProvider& provider, const Settings& settings,
                 std::wstring& errors);
    static bool RefuseClosed(std::wstring& error);

    const std::vector<std::shared_ptr<const VideoEncoderProvider>> m_providers;
    std::unique_ptr<VideoEncoder> m_active;  // the last one opened
    const Settings m_noSettings{};
    const std::wstring m_noName;
};

}  // namespace gazeora
