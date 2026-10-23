#include "gazeora/hook/core/shared_channel.h"

#include "gazeora/hook/core/log.h"

namespace gazeora::hook {

namespace {

HANDLE g_mapping = nullptr;
HANDLE g_frameEvent = nullptr;
SharedHeader* g_shared = nullptr;
const void* g_publisher = nullptr;

}  // namespace

bool OpenSharedChannel() {
    DWORD pid = GetCurrentProcessId();
    g_mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, SharedMemoryName(pid).c_str());
    if (!g_mapping) {
        return false;
    }
    auto* shared = static_cast<SharedHeader*>(
        MapViewOfFile(g_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedHeader)));
    if (!shared || shared->magic != kSharedMagic || shared->version != kSharedVersion) {
        return false;
    }
    g_frameEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, FrameEventName(pid).c_str());
    if (!g_frameEvent) {
        return false;
    }
    g_shared = shared;
    return true;
}

SharedHeader* Shared() { return g_shared; }

bool CaptureEnabled() { return g_shared && g_shared->captureEnabled; }

bool CaptureDepth() { return g_shared && g_shared->captureEnabled && g_shared->captureDepth; }

void SetHookState(HookState state) {
    if (g_shared) {
        g_shared->hookState = state;
    }
}

bool LaunchedByHost() { return g_shared && (g_shared->hostFlags & kHostLaunchedGame); }

void ExportedHandles::Close() {
    for (HANDLE& h : slots) {
        if (h) CloseHandle(h);
        h = nullptr;
    }
    for (HANDLE& h : depthSlots) {
        if (h) CloseHandle(h);
        h = nullptr;
    }
    if (frameFence) CloseHandle(frameFence);
    if (hostFence) CloseHandle(hostFence);
    frameFence = hostFence = nullptr;
}

void PublishResources(const ExportedHandles& handles, UINT width, UINT height, DXGI_FORMAT format,
                      LUID adapterLuid, GraphicsApi api, const void* owner) {
    InterlockedIncrement(&g_shared->texSeq);  // odd: host must not read
    g_shared->width = width;
    g_shared->height = height;
    g_shared->format = format;
    g_shared->api = api;
    g_shared->adapterLuid = adapterLuid;
    for (UINT i = 0; i < kFrameSlots; ++i) {
        g_shared->slotHandles[i] = uint64_t(reinterpret_cast<uintptr_t>(handles.slots[i]));
        g_shared->depthSlotHandles[i] =
            uint64_t(reinterpret_cast<uintptr_t>(handles.depthSlots[i]));
        g_shared->depthSlots[i].frame = 0;
    }
    g_shared->frameFenceHandle = uint64_t(reinterpret_cast<uintptr_t>(handles.frameFence));
    g_shared->hostFenceHandle = uint64_t(reinterpret_cast<uintptr_t>(handles.hostFence));
    g_shared->firstFrame = g_shared->frameIndex + 1;
    InterlockedIncrement(&g_shared->texSeq);  // even: stable again
    g_publisher = owner;

    SetStatus("capturing %ux%u via %s, DXGI format %d", width, height, GraphicsApiName(api),
              format);
}

bool IsPublisher(const void* owner) { return g_publisher == owner; }

UINT64 InitialFenceValue() { return UINT64(g_shared->frameIndex); }

LONG64 NextFrameNumber() { return g_shared->frameIndex + 1; }

bool ClaimSlot(LONG64 frame, UINT64 hostCompleted) {
    InterlockedExchange64(&g_shared->writingFrame, frame);  // full barrier
    LONG64 reading = g_shared->hostReadingFrame;
    bool busy = reading > 0 && reading % kFrameSlots == frame % kFrameSlots &&
                UINT64(reading) > hostCompleted;
    if (busy) {
        CountSkippedFrame();
    }
    return !busy;
}

void CountSkippedFrame() { InterlockedIncrement64(&g_shared->skippedFrames); }

void PublishDepth(LONG64 frame, UINT width, UINT height, DepthEncoding encoding) {
    DepthSlotInfo& slot = g_shared->depthSlots[frame % kFrameSlots];
    slot.width = width;
    slot.height = height;
    slot.encoding = encoding;
    InterlockedExchange64(&slot.frame, frame);
}

void FramePublished(LONG64 frame, LONGLONG presentQpc) {
    InterlockedExchange64(&g_shared->lastPresentQpc, presentQpc);
    InterlockedExchange64(&g_shared->frameIndex, frame);
    g_shared->hookState = kHookCapturing;
    SetEvent(g_frameEvent);
}

}  // namespace gazeora::hook
