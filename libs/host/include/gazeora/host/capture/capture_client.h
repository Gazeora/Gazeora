#pragma once

#include <windows.h>
#include <d3d11_4.h>
#include <wrl/client.h>

#include <string>

#include "gazeora/common/shared.h"

namespace gazeora {

// Host side of the hook: owns the shared memory / frame event for one game process
// and copies each captured frame into a texture on the host's own D3D11 device.
class CaptureClient {
public:
    enum class WaitResult { Frame, Timeout };

    CaptureClient() = default;
    CaptureClient(const CaptureClient&) = delete;
    CaptureClient& operator=(const CaptureClient&) = delete;
    ~CaptureClient();

    // Creates (or reattaches to) the shared state for pid. Call before injecting.
    bool Create(DWORD pid, std::wstring& error);

    // True if the hook is already running in the target from an earlier session.
    bool HookAlreadyLoaded() const {
        return m_alreadyExisted && m_shared->hookState > kHookNotLoaded;
    }

    void SetCaptureEnabled(bool enabled);
    void SetHostFlags(uint32_t flags) { m_shared->hostFlags = flags; }
    const SharedHeader& Shared() const { return *m_shared; }

    // Waits for the hook to signal a frame and copies it into Frame().
    WaitResult WaitForFrame(DWORD timeoutMs);

    // Valid after WaitForFrame returned Frame. Lives on Device().
    ID3D11Texture2D* Frame() const { return m_frame.Get(); }
    ID3D11Device* Device() const { return m_device.Get(); }

    // Time from the game's Present to the host receiving the frame, for the last frame.
    double LastHandoffMs() const { return m_lastHandoffMs; }

private:
    bool SyncResources();
    bool OpenResources(const SharedHeader& snapshot);
    bool CreateDeviceForAdapter(const LUID& luid);
    HANDLE DuplicateFromGame(uint64_t handle);
    void ReleaseResources();

    HANDLE m_mapping = nullptr;
    HANDLE m_frameEvent = nullptr;
    HANDLE m_gameProcess = nullptr;
    SharedHeader* m_shared = nullptr;
    bool m_alreadyExisted = false;

    Microsoft::WRL::ComPtr<ID3D11Device5> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> m_context;
    LUID m_deviceLuid = {};

    LONG m_openedSeq = 0;
    int64_t m_firstFrame = 0;
    int64_t m_lastFrame = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_slots[kFrameSlots];
    Microsoft::WRL::ComPtr<ID3D11Fence> m_frameFence;
    Microsoft::WRL::ComPtr<ID3D11Fence> m_hostFence;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_frame;

    LONGLONG m_qpcFreq = 1;
    double m_lastHandoffMs = 0.0;
};

}  // namespace gazeora
