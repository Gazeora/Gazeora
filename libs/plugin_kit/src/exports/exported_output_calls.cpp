// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_kit/exports/exported_output_calls.h"

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugin_kit/utils/plugin_text.h"

namespace gazeora {

namespace {

int32_t Bool(bool value) { return value ? GAZEORA_TRUE : GAZEORA_FALSE; }

}  // namespace

void ExportedOutputCalls::Fill(std::unique_ptr<ExportedOutput> exported, GazeoraOutputV2& output) {
    output = {sizeof(GazeoraOutputV2),
              exported.release(),
              Start,
              Stop,
              Port,
              Viewers,
              TakeKeyFrameRequest,
              Send,
              Url,
              Status,
              Destroy,
              AppViewers,
              Watching,
              NeedsUsbDriver,
              UsbDevicesNeedingDriver};
}

int32_t GAZEORA_CALL ExportedOutputCalls::Start(void* self, uint32_t ipv4, uint16_t port,
                                                char* error, size_t capacity) {
    std::string text;
    const bool started = Of(self).Start(ipv4, port, text);
    PluginText::Write(text, error, capacity);
    return Bool(started);
}

void GAZEORA_CALL ExportedOutputCalls::Stop(void* self) { Of(self).Output().Stop(); }

uint16_t GAZEORA_CALL ExportedOutputCalls::Port(void* self) { return Of(self).Output().Port(); }

int32_t GAZEORA_CALL ExportedOutputCalls::Viewers(void* self) {
    return Of(self).Output().Viewers();
}

int32_t GAZEORA_CALL ExportedOutputCalls::TakeKeyFrameRequest(void* self) {
    return Bool(Of(self).Output().TakeKeyFrameRequest());
}

void GAZEORA_CALL ExportedOutputCalls::Send(void* self, const GazeoraFrameV2* frame,
                                            const GazeoraLossReporterV2* loss) {
    if (frame && loss) {
        Of(self).Send(*frame, *loss);
    }
}

void GAZEORA_CALL ExportedOutputCalls::Url(void* self, const char* host, char* url,
                                           size_t capacity) {
    const std::wstring wide = Of(self).Output().Url(PluginText::Wide(host));
    PluginText::Write(Utf8::FromWide(wide), url, capacity);
}

void GAZEORA_CALL ExportedOutputCalls::Status(void* self, char* status, size_t capacity) {
    PluginText::Write(Utf8::FromWide(Of(self).Output().Status()), status, capacity);
}

void GAZEORA_CALL ExportedOutputCalls::Destroy(void* self) { delete &Of(self); }

int32_t GAZEORA_CALL ExportedOutputCalls::AppViewers(void* self) {
    return Of(self).Output().AppViewers();
}

int32_t GAZEORA_CALL ExportedOutputCalls::Watching(void* self, uint32_t ipv4) {
    in_addr address = {};
    address.s_addr = ipv4;
    return Bool(Of(self).Output().Watching(address));
}

int32_t GAZEORA_CALL ExportedOutputCalls::NeedsUsbDriver(void* self) {
    return Bool(Of(self).Output().NeedsUsbDriver());
}

void GAZEORA_CALL ExportedOutputCalls::UsbDevicesNeedingDriver(void* self,
                                                               GazeoraUsbDeviceSinkV2 sink,
                                                               void* sinkContext) {
    if (!sink) {
        return;
    }
    for (const UsbPhone& phone : Of(self).Output().PhonesNeedingDriver()) {
        sink(sinkContext, Utf8::FromWide(phone.hardwareId).c_str(),
             Utf8::FromWide(phone.name).c_str());
    }
}

}  // namespace gazeora
