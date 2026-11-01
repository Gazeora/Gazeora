// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "adapters/plugin_frame_output.h"

#include "gazeora/plugin_kit/utils/plugin_struct.h"
#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

PluginFrameOutput::PluginFrameOutput(std::shared_ptr<const IPluginModule> module,
                                     const GazeoraProtocolV2& protocol, StreamLog& log)
    : m_module(std::move(module)), m_name(PluginText::Wide(protocol.name)), m_log(log) {
    m_host = {sizeof(m_host), this, Log, Input};
    GazeoraOutputV2 made = {};
    made.struct_size = sizeof(made);
    if (protocol.create_output(protocol.self, &m_host, &made) == GAZEORA_TRUE) {
        // A plugin built before the optional functions has a shorter struct.
        m_output = PluginStruct::Copy<GazeoraOutputV2>(&made, made.struct_size);
        m_ready = Valid(m_output);
    }
}

PluginFrameOutput::~PluginFrameOutput() {
    Stop();
    if (m_output.destroy) {
        m_output.destroy(m_output.self);
    }
}

bool PluginFrameOutput::Valid(const GazeoraOutputV2& output) {
    return output.start && output.stop && output.port && output.viewers &&
           output.take_key_frame_request && output.send && output.url && output.destroy;
}

bool PluginFrameOutput::Start(in_addr address, uint16_t port, std::wstring& error) {
    if (!m_ready) {
        error = m_name + L" couldn't make an output";
        return false;
    }
    PluginText text;
    if (m_output.start(m_output.self, address.s_addr, port, text.Data(), text.Capacity()) !=
        GAZEORA_TRUE) {
        error = text.Wide().empty() ? m_name + L" couldn't start" : text.Wide();
        return false;
    }
    m_started = true;
    return true;
}

void PluginFrameOutput::Stop() {
    if (m_started) {
        m_output.stop(m_output.self);
        m_started = false;
    }
}

uint16_t PluginFrameOutput::Port() const { return m_ready ? m_output.port(m_output.self) : 0; }

int PluginFrameOutput::Viewers() const { return m_ready ? m_output.viewers(m_output.self) : 0; }

int PluginFrameOutput::AppViewers() const {
    return m_ready && m_output.app_viewers ? m_output.app_viewers(m_output.self) : Viewers();
}

bool PluginFrameOutput::TakeKeyFrameRequest() {
    return m_ready && m_output.take_key_frame_request(m_output.self) == GAZEORA_TRUE;
}

void PluginFrameOutput::Send(const VideoPipeline::EncodedFrame& frame, LossReportSink& loss,
                             uint64_t now) {
    if (!m_started) {
        return;
    }
    const std::string& codec = frame.codec.Id();
    GazeoraFrameV2 plugin = {sizeof(plugin),
                             reinterpret_cast<const uint8_t*>(frame.annexB.data()),
                             frame.annexB.size(),
                             frame.timestamp,
                             frame.keyFrame ? GAZEORA_TRUE : GAZEORA_FALSE,
                             codec.c_str(),
                             frame.width,
                             frame.height,
                             frame.bitrate,
                             frame.fps,
                             &m_codecs.For(frame.codec, frame.format)};
    LossContext context{loss, now};
    const GazeoraLossReporterV2 reporter = {&context, ReportLoss};
    m_output.send(m_output.self, &plugin, &reporter);
}

std::wstring PluginFrameOutput::Url(const std::wstring& host) const {
    if (!m_ready) {
        return {};
    }
    PluginText url;
    m_output.url(m_output.self, Utf8::FromWide(host).c_str(), url.Data(), url.Capacity());
    return url.Wide();
}

std::wstring PluginFrameOutput::Status() const {
    if (!m_ready || !m_output.status) {
        return {};
    }
    PluginText status;
    m_output.status(m_output.self, status.Data(), status.Capacity());
    return status.Wide();
}

bool PluginFrameOutput::NeedsUsbDriver() const {
    return m_ready && m_output.needs_usb_driver &&
           m_output.needs_usb_driver(m_output.self) == GAZEORA_TRUE;
}

std::vector<UsbPhone> PluginFrameOutput::PhonesNeedingDriver() const {
    std::vector<UsbPhone> phones;
    if (m_ready && m_output.usb_devices_needing_driver) {
        m_output.usb_devices_needing_driver(m_output.self, AddPhone, &phones);
    }
    return phones;
}

bool PluginFrameOutput::Watching(const in_addr& address) {
    return m_ready && m_output.watching &&
           m_output.watching(m_output.self, address.s_addr) == GAZEORA_TRUE;
}

void GAZEORA_CALL PluginFrameOutput::Log(void* self, const char* message) {
    if (self && message) {
        static_cast<PluginFrameOutput*>(self)->m_log.Write("%s", message);
    }
}

void GAZEORA_CALL PluginFrameOutput::Input(void* self, uint32_t type, const uint8_t* payload,
                                           size_t size) {
    if (self) {
        static_cast<PluginFrameOutput*>(self)->m_input.Route(type, payload, size);
    }
}

void GAZEORA_CALL PluginFrameOutput::ReportLoss(void* context, uint64_t player,
                                                float fractionLost) {
    if (context) {
        auto* loss = static_cast<LossContext*>(context);
        loss->sink.Report(player, fractionLost, loss->now);
    }
}

void GAZEORA_CALL PluginFrameOutput::AddPhone(void* phones, const char* hardwareId,
                                              const char* name) {
    if (phones && hardwareId) {
        static_cast<std::vector<UsbPhone>*>(phones)->push_back(
            {PluginText::Wide(hardwareId), PluginText::Wide(name)});
    }
}

}  // namespace gazeora
