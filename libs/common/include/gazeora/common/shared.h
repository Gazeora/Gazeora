// Layout of the shared-memory block used by the host (Gazeora.exe) and the hook DLL
// running inside the game. The host creates it before injecting; the hook opens it.
//
// Frame transport (identical for every graphics API):
//  - The hook owns a ring of kFrameSlots shared textures plus two shared fences, all
//    exported as NT handles that the host duplicates out of the game process. D3D11
//    and D3D12 games create them on their own device; other APIs (D3D8/9/10, OpenGL,
//    Vulkan) get a private D3D11 device inside the game that owns them.
//  - Frame N is copied into slot N % kFrameSlots; the hook's GPU signals frameFence = N
//    once the copy is done, and the hook publishes frameIndex = N.
//  - The host GPU-waits on frameFence >= N, copies the slot out, then signals
//    hostFence = N. The hook only reuses a slot once the host is done with it.
//  - writingFrame / hostReadingFrame form a Dekker-style handshake so the hook
//    never overwrites a slot the host has just decided to read (both sides write
//    their own field with a full barrier, then read the other's).
//  - For 3D from depth (captureDepth), D3D11 and D3D12 games also copy the game's depth
//    buffer into a depth slot beside each frame slot: RGBA8 textures the frame's size, each
//    texel one depth value as a little-endian 32-bit word (DepthEncoding). They're written
//    with the frame and covered by its fence; depthSlots says what each one holds.
#pragma once

#include <windows.h>
#include <cstdint>
#include <cwchar>
#include <string>

namespace gazeora {

constexpr uint32_t kSharedMagic = 0x415A4147;  // "GAZA"
constexpr uint32_t kSharedVersion = 4;
constexpr uint32_t kFrameSlots = 3;

enum HookState : LONG {
    kHookNotLoaded = 0,
    kHookInitializing = 1,
    kHookInstalled = 2,  // Present is hooked, waiting for the game to present
    kHookCapturing = 3,  // at least one frame has been copied
    kHookError = -1,
};

// SharedHeader::api: which graphics API the game presents with.
enum GraphicsApi : uint32_t {
    kApiUnknown = 0,
    kApiD3D8 = 8,
    kApiD3D9 = 9,
    kApiD3D10 = 10,
    kApiD3D11 = 11,
    kApiD3D12 = 12,
    kApiOpenGL = 100,
    kApiVulkan = 101,
};

inline const char* GraphicsApiName(uint32_t api) {
    switch (api) {
        case kApiD3D8:
            return "D3D8";
        case kApiD3D9:
            return "D3D9";
        case kApiD3D10:
            return "D3D10";
        case kApiD3D11:
            return "D3D11";
        case kApiD3D12:
            return "D3D12";
        case kApiOpenGL:
            return "OpenGL";
        case kApiVulkan:
            return "Vulkan";
        default:
            return "unknown API";
    }
}

// SharedHeader::hostFlags.
enum HostFlags : uint32_t {
    // The host started the game suspended and injected before any game code ran, so
    // the hook may load graphics runtimes itself to hook them before the game does.
    kHostLaunchedGame = 1,
};

// How a depth slot's texels hold the depth buffer's values, read as 32-bit words.
enum DepthEncoding : uint32_t {
    kDepthNone = 0,
    kDepthFloat = 1,    // a 32-bit float, 0..1
    kDepthUnorm24 = 2,  // the low 24 bits, 0..0xFFFFFF for 0..1 (the stencil in the top byte)
};

// What one depth slot holds.
struct DepthSlotInfo {
    volatile LONG64 frame;  // the frame whose depth it holds; 0 for none
    uint32_t width;         // the depth buffer's size; the slot holds as much as fits, from
    uint32_t height;        // the top left
    uint32_t encoding;      // DepthEncoding
    uint32_t reserved;
};

struct SharedHeader {
    uint32_t magic;
    uint32_t version;
    volatile LONG hookState;       // HookState, written by the hook
    volatile LONG captureEnabled;  // written by the host; hook skips copying when 0
    volatile LONG captureDepth;    // written by the host; the depth buffer too, for 3D

    // Seqlock around the texture description below: odd while the hook is
    // rewriting it, 0 until the first textures exist.
    volatile LONG texSeq;
    uint32_t width;
    uint32_t height;
    uint32_t format;                         // DXGI_FORMAT
    uint32_t api;                            // GraphicsApi
    uint32_t hostFlags;                      // HostFlags, written by the host before injecting
    LUID adapterLuid;                        // the host must open the textures on this adapter
    uint64_t slotHandles[kFrameSlots];       // NT handles, valid in the game process
    uint64_t frameFenceHandle;               // NT handle, valid in the game process
    uint64_t hostFenceHandle;                // NT handle, valid in the game process
    uint64_t depthSlotHandles[kFrameSlots];  // NT handles; 0 without depth slots
    int64_t firstFrame;                      // first frame number written to these textures

    volatile LONG64 frameIndex;             // latest published frame number (written by hook)
    volatile LONG64 writingFrame;           // frame the hook is about to write (hook)
    volatile LONG64 hostReadingFrame;       // frame the host is about to read (host)
    volatile LONG64 lastPresentQpc;         // QueryPerformanceCounter at capture time
    volatile LONG64 skippedFrames;          // presents not captured because a slot was busy
    DepthSlotInfo depthSlots[kFrameSlots];  // by slot, written by the hook with the frame

    char status[256];  // last human-readable status/error from the hook
};

inline std::wstring SharedMemoryName(DWORD pid) {
    wchar_t name[64];
    swprintf(name, 64, L"Local\\Gazeora_shm_%lu", pid);
    return name;
}

inline std::wstring FrameEventName(DWORD pid) {
    wchar_t name[64];
    swprintf(name, 64, L"Local\\Gazeora_frame_%lu", pid);
    return name;
}

}  // namespace gazeora
