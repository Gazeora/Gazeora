// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "adapters/plugin_input_router.h"
#include "gazeora/host/stream/frame_output.h"
#include "gazeora/host/stream/stream_log.h"
#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_kit/exports/exported_codec_cache.h"
#include "gazeora/plugins/interfaces/plugin_module.h"

namespace gazeora {

// Adapter pattern: a plugin protocol's GazeoraOutputV2 behind FrameOutput. Asks the plugin for
// the output when it's made; if the plugin can't make one, Start says so. Keeps the DLL loaded.
class PluginFrameOutput final : public FrameOutput {
public:
    // `protocol` must have passed PluginRegistrar's checks. `log` outlives the output.
    PluginFrameOutput(std::shared_ptr<const IPluginModule> module,
                      const GazeoraProtocolV2& protocol, StreamLog& log);
    PluginFrameOutput(const PluginFrameOutput&) = delete;
    PluginFrameOutput& operator=(const PluginFrameOutput&) = delete;
    ~PluginFrameOutput() override;

    // Whether the plugin filled in every function an output must have.
    static bool Valid(const GazeoraOutputV2& output);

    bool Start(in_addr address, uint16_t port, std::wstring& error) override;
    void Stop() override;
    uint16_t Port() const override;
    int Viewers() const override;
    int AppViewers() const override;
    bool TakeKeyFrameRequest() override;
    void Send(const VideoPipeline::EncodedFrame& frame, LossReportSink& loss,
              uint64_t now) override;
    std::wstring Url(const std::wstring& host) const override;
    std::wstring Status() const override;
    bool NeedsUsbDriver() const override;
    std::vector<UsbPhone> PhonesNeedingDriver() const override;
    void SetPointerSink(PointerEventSink* sink) override { m_input.SetPointerSink(sink); }
    void SetHeadPoseSink(HeadPoseSink* sink) override { m_input.SetHeadPoseSink(sink); }
    void SetCameraStatusSink(CameraStatusSink* sink) override { m_input.SetCameraStatusSink(sink); }
    bool Watching(const in_addr& address) override;

private:
    // What a loss report needs to reach the stream's sink, during one Send.
    struct LossContext {
        LossReportSink& sink;
        uint64_t now;
    };

    static void GAZEORA_CALL Log(void* self, const char* message);
    static void GAZEORA_CALL Input(void* self, uint32_t type, const uint8_t* payload, size_t size);
    static void GAZEORA_CALL ReportLoss(void* context, uint64_t player, float fractionLost);
    static void GAZEORA_CALL AddPhone(void* phones, const char* hardwareId, const char* name);

    const std::shared_ptr<const IPluginModule> m_module;
    const std::wstring m_name;
    StreamLog& m_log;
    PluginInputRouter m_input;
    GazeoraOutputHostV2 m_host = {};  // the plugin may keep a pointer to it
    GazeoraOutputV2 m_output = {};
    bool m_ready = false;
    bool m_started = false;
    ExportedCodecCache m_codecs;  // Send's
};

}  // namespace gazeora
