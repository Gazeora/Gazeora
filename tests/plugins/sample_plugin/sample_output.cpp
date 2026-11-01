// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>

#include "sample_parts.h"

namespace sample {

namespace {

constexpr uint16_t kPort = 4321;
constexpr uint64_t kPlayer = 7;

// What the tests read back; the DLL is loaded once per test process.
std::atomic<int> g_framesSent{0};
std::mutex g_codecMutex;
std::string g_lastCodec;

struct SampleOutput {
    const GazeoraOutputHostV2* host = nullptr;
    std::atomic<bool> started{false};
    std::atomic<bool> keyFrameWanted{false};
};

SampleOutput& Of(void* self) { return *static_cast<SampleOutput*>(self); }

int32_t GAZEORA_CALL Start(void* self, uint32_t, uint16_t, char*, size_t) {
    Of(self).started = true;
    Of(self).keyFrameWanted = true;
    Of(self).host->log(Of(self).host->context, "sample output started");
    return GAZEORA_TRUE;
}

void GAZEORA_CALL Stop(void* self) { Of(self).started = false; }
uint16_t GAZEORA_CALL Port(void*) { return kPort; }
int32_t GAZEORA_CALL Viewers(void* self) { return Of(self).started ? 1 : 0; }

int32_t GAZEORA_CALL TakeKeyFrameRequest(void* self) {
    return Of(self).keyFrameWanted.exchange(false) ? GAZEORA_TRUE : GAZEORA_FALSE;
}

void GAZEORA_CALL Send(void*, const GazeoraFrameV2* frame, const GazeoraLossReporterV2* loss) {
    {
        std::lock_guard<std::mutex> lock(g_codecMutex);
        g_lastCodec = frame->codec_id;
    }
    ++g_framesSent;
    loss->report(loss->context, kPlayer, 0.0f);
}

void GAZEORA_CALL Url(void*, const char* host, char* url, size_t capacity) {
    std::snprintf(url, capacity, "sample://%s/", host);
}

void GAZEORA_CALL Status(void*, char* status, size_t capacity) {
    Copy("sample status", status, capacity);
}

void GAZEORA_CALL Destroy(void* self) { delete &Of(self); }

int32_t GAZEORA_CALL CreateOutput(void*, const GazeoraOutputHostV2* host, GazeoraOutputV2* output) {
    auto* sampleOutput = new SampleOutput();
    sampleOutput->host = host;
    *output = {sizeof(GazeoraOutputV2), sampleOutput, Start, Stop,   Port,   Viewers,
               TakeKeyFrameRequest,     Send,         Url,   Status, Destroy};
    return GAZEORA_TRUE;
}

const GazeoraProtocolV2 kProtocol = {
    sizeof(GazeoraProtocolV2),       nullptr, "sample-out",  "Sample output",
    "Sample output: for the tests.", "/",     GAZEORA_FALSE, CreateOutput};

}  // namespace

const GazeoraProtocolV2& Protocol() { return kProtocol; }

int FramesSent() { return g_framesSent; }

const char* LastCodec() {
    static thread_local std::string copy;
    std::lock_guard<std::mutex> lock(g_codecMutex);
    copy = g_lastCodec;
    return copy.c_str();
}

}  // namespace sample
