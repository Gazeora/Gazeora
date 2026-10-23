#include "gazeora/host/capture/capture_client.h"

#include <dxgi1_2.h>

#include <cstdio>

using Microsoft::WRL::ComPtr;

namespace gazeora {

CaptureClient::~CaptureClient() {
    ReleaseResources();
    if (m_shared) {
        m_shared->captureEnabled = 0;
        InterlockedExchange64(&m_shared->hostReadingFrame, 0);
        UnmapViewOfFile(m_shared);
    }
    if (m_frameEvent) {
        CloseHandle(m_frameEvent);
    }
    if (m_mapping) {
        CloseHandle(m_mapping);
    }
    if (m_gameProcess) {
        CloseHandle(m_gameProcess);
    }
}

bool CaptureClient::Create(DWORD pid, std::wstring& error) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_qpcFreq = freq.QuadPart;

    // The hook exports its textures and fences as NT handles local to the game;
    // we duplicate them into this process.
    m_gameProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
    if (!m_gameProcess) {
        error = L"OpenProcess(PROCESS_DUP_HANDLE) failed (error " +
                std::to_wstring(GetLastError()) + L")";
        return false;
    }

    m_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                   sizeof(SharedHeader), SharedMemoryName(pid).c_str());
    if (!m_mapping) {
        error = L"CreateFileMapping failed (error " + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    // The hook keeps its own handle open, so the block survives a host restart.
    m_alreadyExisted = GetLastError() == ERROR_ALREADY_EXISTS;

    m_shared = static_cast<SharedHeader*>(
        MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedHeader)));
    if (!m_shared) {
        error = L"MapViewOfFile failed (error " + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    if (!m_alreadyExisted || m_shared->magic != kSharedMagic) {
        ZeroMemory(m_shared, sizeof(SharedHeader));
        m_shared->magic = kSharedMagic;
        m_shared->version = kSharedVersion;
        m_alreadyExisted = false;
    } else if (m_shared->version != kSharedVersion) {
        error = L"an older Gazeora hook is already loaded in this game; restart the game first";
        UnmapViewOfFile(m_shared);
        m_shared = nullptr;
        return false;
    }

    m_frameEvent = CreateEventW(nullptr, FALSE, FALSE, FrameEventName(pid).c_str());
    if (!m_frameEvent) {
        error = L"CreateEvent failed (error " + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    return true;
}

void CaptureClient::SetCaptureEnabled(bool enabled) {
    InterlockedExchange(&m_shared->captureEnabled, enabled ? 1 : 0);
}

void CaptureClient::SetCaptureDepth(bool depth) {
    InterlockedExchange(&m_shared->captureDepth, depth ? 1 : 0);
}

bool CaptureClient::CreateDeviceForAdapter(const LUID& luid) {
    m_device.Reset();
    m_context.Reset();

    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        return false;
    }

    // Shared resources only open on the adapter that created them, which on
    // hybrid-GPU laptops is often not the default adapter.
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.AdapterLuid.LowPart == luid.LowPart &&
            desc.AdapterLuid.HighPart == luid.HighPart) {
            break;
        }
        adapter.Reset();
    }
    if (!adapter) {
        return false;
    }

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    HRESULT hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                                   &device, nullptr, &context);
    if (FAILED(hr) || FAILED(device.As(&m_device)) || FAILED(context.As(&m_context))) {
        m_device.Reset();
        m_context.Reset();
        return false;
    }
    m_deviceLuid = luid;
    return true;
}

HANDLE CaptureClient::DuplicateFromGame(uint64_t handle) {
    HANDLE local = nullptr;
    DuplicateHandle(m_gameProcess, reinterpret_cast<HANDLE>(handle), GetCurrentProcess(), &local, 0,
                    FALSE, DUPLICATE_SAME_ACCESS);
    return local;
}

void CaptureClient::ReleaseResources() {
    for (auto& slot : m_slots) slot.Reset();
    for (auto& slot : m_depthSlots) slot.Reset();
    m_frameFence.Reset();
    m_hostFence.Reset();
    m_frame.Reset();
    m_depthFrame.Reset();
    m_depth = DepthFrame();
    m_openedSeq = 0;
}

bool CaptureClient::OpenResources(const SharedHeader& snapshot) {
    const LUID& luid = snapshot.adapterLuid;
    if (!m_device || luid.LowPart != m_deviceLuid.LowPart ||
        luid.HighPart != m_deviceLuid.HighPart) {
        if (!CreateDeviceForAdapter(luid)) {
            fprintf(stderr, "Could not create a D3D11.4 device on the game's GPU\n");
            return false;
        }
    }

    // Duplicate everything first, then confirm the hook didn't replace the resources
    // meanwhile (it closes the old handles, and the values could be reused).
    HANDLE handles[2 * kFrameSlots + 2] = {};
    HANDLE* depthHandles = handles + kFrameSlots + 2;
    for (UINT i = 0; i < kFrameSlots; ++i) {
        handles[i] = DuplicateFromGame(snapshot.slotHandles[i]);
        if (snapshot.depthSlotHandles[i]) {
            depthHandles[i] = DuplicateFromGame(snapshot.depthSlotHandles[i]);
        }
    }
    handles[kFrameSlots] = DuplicateFromGame(snapshot.frameFenceHandle);
    handles[kFrameSlots + 1] = DuplicateFromGame(snapshot.hostFenceHandle);
    bool ok = m_shared->texSeq == m_openedSeq;  // m_openedSeq holds the snapshot's seq here

    for (UINT i = 0; ok && i < kFrameSlots; ++i) {
        ok = handles[i] &&
             SUCCEEDED(m_device->OpenSharedResource1(handles[i], IID_PPV_ARGS(&m_slots[i])));
        // Without depth the frames still go out, in 2D.
        if (ok && depthHandles[i] &&
            FAILED(m_device->OpenSharedResource1(depthHandles[i],
                                                 IID_PPV_ARGS(&m_depthSlots[i])))) {
            m_depthSlots[i].Reset();
        }
    }
    ok = ok && handles[kFrameSlots] &&
         SUCCEEDED(m_device->OpenSharedFence(handles[kFrameSlots], IID_PPV_ARGS(&m_frameFence)));
    ok = ok && handles[kFrameSlots + 1] &&
         SUCCEEDED(m_device->OpenSharedFence(handles[kFrameSlots + 1], IID_PPV_ARGS(&m_hostFence)));
    for (HANDLE h : handles) {
        if (h) CloseHandle(h);
    }
    if (!ok) {
        return false;
    }

    D3D11_TEXTURE2D_DESC desc;
    m_slots[0]->GetDesc(&desc);
    desc.MiscFlags = 0;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &m_frame))) {
        return false;
    }
    if (m_depthSlots[0] && m_depthSlots[1] && m_depthSlots[2]) {
        D3D11_TEXTURE2D_DESC depthDesc;
        m_depthSlots[0]->GetDesc(&depthDesc);
        depthDesc.MiscFlags = 0;
        depthDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthFrame))) {
            m_depthFrame.Reset();
        }
    }
    printf("Attached to %s game frames %ux%u, DXGI format %u\n", GraphicsApiName(snapshot.api),
           desc.Width, desc.Height, desc.Format);
    return true;
}

bool CaptureClient::SyncResources() {
    LONG seq = m_shared->texSeq;
    if (seq == 0 || (seq & 1)) {
        return false;  // no textures yet, or the hook is mid-update
    }
    if (seq == m_openedSeq && m_frame) {
        return true;
    }

    MemoryBarrier();
    SharedHeader snapshot = *const_cast<const SharedHeader*>(m_shared);
    MemoryBarrier();
    if (m_shared->texSeq != seq) {
        return false;
    }

    ReleaseResources();
    m_openedSeq = seq;
    if (!OpenResources(snapshot)) {
        ReleaseResources();
        return false;
    }
    m_firstFrame = snapshot.firstFrame;
    return true;
}

CaptureClient::WaitResult CaptureClient::WaitForFrame(DWORD timeoutMs) {
    if (WaitForSingleObject(m_frameEvent, timeoutMs) != WAIT_OBJECT_0) {
        return WaitResult::Timeout;
    }
    if (!SyncResources()) {
        return WaitResult::Timeout;
    }

    LONG64 frame = m_shared->frameIndex;
    if (frame <= m_lastFrame || frame < m_firstFrame) {
        return WaitResult::Timeout;
    }

    // Handshake with ClaimSlot in the hook: announce the read, then make sure the
    // hook hasn't already started overwriting this slot with a newer frame.
    InterlockedExchange64(&m_shared->hostReadingFrame, frame);  // full barrier
    if (m_shared->writingFrame >= frame + LONG64(kFrameSlots)) {
        InterlockedExchange64(&m_shared->hostReadingFrame, 0);
        return WaitResult::Timeout;
    }

    // All GPU-side: wait for the hook's copy, copy the slot out, tell the hook we're done.
    m_context->Wait(m_frameFence.Get(), UINT64(frame));
    m_context->CopyResource(m_frame.Get(), m_slots[frame % kFrameSlots].Get());
    // The depth slot holds this frame's depth only if the hook says so.
    const DepthSlotInfo& depth = m_shared->depthSlots[frame % kFrameSlots];
    m_depth = DepthFrame();
    if (m_depthFrame && depth.frame == frame && depth.encoding != kDepthNone) {
        m_context->CopyResource(m_depthFrame.Get(), m_depthSlots[frame % kFrameSlots].Get());
        m_depth.texture = m_depthFrame.Get();
        m_depth.width = depth.width;
        m_depth.height = depth.height;
        m_depth.encoding = DepthEncoding(depth.encoding);
    }
    m_context->Signal(m_hostFence.Get(), UINT64(frame));
    m_context->Flush();
    m_lastFrame = frame;

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    m_lastHandoffMs = double(now.QuadPart - m_shared->lastPresentQpc) * 1000.0 / double(m_qpcFreq);
    return WaitResult::Frame;
}

}  // namespace gazeora
