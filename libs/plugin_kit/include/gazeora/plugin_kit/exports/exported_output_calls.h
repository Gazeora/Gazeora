// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <memory>

#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_kit/exports/exported_output.h"

namespace gazeora {

// Adapter pattern: the GazeoraOutputV2 functions of an ExportedOutput, each one calling it.
class ExportedOutputCalls final {
public:
    ExportedOutputCalls() = delete;

    // Fills `output` with `exported`'s functions; output.destroy frees it.
    static void Fill(std::unique_ptr<ExportedOutput> exported, GazeoraOutputV2& output);

private:
    static ExportedOutput& Of(void* self) { return *static_cast<ExportedOutput*>(self); }

    static int32_t GAZEORA_CALL Start(void* self, uint32_t ipv4, uint16_t port, char* error,
                                      size_t capacity);
    static void GAZEORA_CALL Stop(void* self);
    static uint16_t GAZEORA_CALL Port(void* self);
    static int32_t GAZEORA_CALL Viewers(void* self);
    static int32_t GAZEORA_CALL TakeKeyFrameRequest(void* self);
    static void GAZEORA_CALL Send(void* self, const GazeoraFrameV2* frame,
                                  const GazeoraLossReporterV2* loss);
    static void GAZEORA_CALL Url(void* self, const char* host, char* url, size_t capacity);
    static void GAZEORA_CALL Status(void* self, char* status, size_t capacity);
    static void GAZEORA_CALL Destroy(void* self);
    static int32_t GAZEORA_CALL AppViewers(void* self);
    static int32_t GAZEORA_CALL Watching(void* self, uint32_t ipv4);
    static int32_t GAZEORA_CALL NeedsUsbDriver(void* self);
    static void GAZEORA_CALL UsbDevicesNeedingDriver(void* self, GazeoraUsbDeviceSinkV2 sink,
                                                     void* sinkContext);
};

}  // namespace gazeora
