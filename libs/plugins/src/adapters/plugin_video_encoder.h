// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>

#include "gazeora/host/encode/video_encoder.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugins/interfaces/plugin_module.h"

namespace gazeora {

// Adapter pattern: a plugin's GazeoraEncoderV1 behind VideoEncoder. Keeps the DLL loaded and
// destroys the plugin's encoder with it.
class PluginVideoEncoder final : public VideoEncoder {
public:
    // `encoder` must have passed Valid(). `knowsIds`: its provider is a version 2 one, which
    // takes codecs by id (GazeoraEncoderSettingsV2).
    PluginVideoEncoder(std::shared_ptr<const IPluginModule> module, std::wstring name,
                       const GazeoraEncoderV1& encoder, bool knowsIds);
    PluginVideoEncoder(const PluginVideoEncoder&) = delete;
    PluginVideoEncoder& operator=(const PluginVideoEncoder&) = delete;
    ~PluginVideoEncoder() override;

    // Whether the plugin filled in every function an encoder must have.
    static bool Valid(const GazeoraEncoderV1& encoder);

    bool Open(const Settings& settings, std::wstring& error) override;
    void Close() override;
    bool IsOpen() const override { return m_open; }
    const Settings& Current() const override { return m_settings; }
    const std::wstring& Name() const override { return m_name; }
    bool IsHardware() const override;
    void SetBitrate(uint32_t bitrate) override;
    void RequestKeyFrame() override;
    bool Encode(const uint8_t* nv12, int64_t time, std::vector<Frame>& out,
                std::wstring& error) override;
    bool EncodeTexture(ID3D11Texture2D* nv12, int64_t time, std::vector<Frame>& out,
                       std::wstring& error) override;

private:
    static void GAZEORA_CALL CollectFrame(void* context, const GazeoraEncodedFrameV1* frame);
    // `settings` has to outlive the result, which points at its codec id.
    GazeoraEncoderSettingsV2 ToPlugin(const Settings& settings) const;
    // `error` from the plugin's text, or a default when it gave none.
    bool Failed(const std::wstring& text, const wchar_t* what, std::wstring& error) const;

    const std::shared_ptr<const IPluginModule> m_module;
    const std::wstring m_name;
    const GazeoraEncoderV1 m_encoder;
    const bool m_knowsIds;
    Settings m_settings;
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;  // kept alive while Current() names it
    bool m_open = false;
};

}  // namespace gazeora
