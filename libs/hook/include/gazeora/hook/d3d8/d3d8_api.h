// Minimal Direct3D 8 declarations. The Windows SDK no longer ships d3d8.h, so this
// declares just what the hook calls, by vtable slot. Types D3D8 shares with D3D9
// (D3DFORMAT, D3DLOCKED_RECT, ...) kept their values and come from d3d9types.h.
#pragma once

#include <windows.h>
#include <d3d9types.h>

namespace gazeora::hook::d3d8 {

constexpr UINT kSdkVersion = 220;
constexpr UINT kAdapterDefault = 0;                      // D3DADAPTER_DEFAULT
constexpr DWORD kCreateSoftwareVertexProcessing = 0x20;  // D3DCREATE_SOFTWARE_VERTEXPROCESSING

// Only IUnknown (vtable slots 0-2) is declared, so these work with ComPtr.
struct Direct3D8 : IUnknown {};
struct Device8 : IUnknown {};
struct Surface8 : IUnknown {};

// D3DSURFACE_DESC as of D3D8 (D3D9 dropped Size and added MultiSampleQuality).
struct SurfaceDesc {
    D3DFORMAT Format;
    D3DRESOURCETYPE Type;
    DWORD Usage;
    D3DPOOL Pool;
    UINT Size;
    D3DMULTISAMPLE_TYPE MultiSampleType;
    UINT Width;
    UINT Height;
};

// D3DPRESENT_PARAMETERS as of D3D8.
struct PresentParameters {
    UINT BackBufferWidth;
    UINT BackBufferHeight;
    D3DFORMAT BackBufferFormat;
    UINT BackBufferCount;
    D3DMULTISAMPLE_TYPE MultiSampleType;
    D3DSWAPEFFECT SwapEffect;
    HWND hDeviceWindow;
    BOOL Windowed;
    BOOL EnableAutoDepthStencil;
    D3DFORMAT AutoDepthStencilFormat;
    DWORD Flags;
    UINT FullScreen_RefreshRateInHz;
    UINT FullScreen_PresentationInterval;
};

using Direct3DCreate8Fn = Direct3D8*(WINAPI*)(UINT);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(Device8*, const RECT*, const RECT*, HWND,
                                              const RGNDATA*);
using ResetFn = HRESULT(STDMETHODCALLTYPE*)(Device8*, PresentParameters*);

// IDirect3D8: IUnknown 0-2, then d3d8.h order.
constexpr size_t kGetAdapterDisplayModeSlot = 8;
constexpr size_t kCreateDeviceSlot = 15;
// IDirect3DDevice8: IUnknown 0-2, then d3d8.h order.
constexpr size_t kResetSlot = 14;
constexpr size_t kPresentSlot = 15;
constexpr size_t kGetBackBufferSlot = 16;
constexpr size_t kCreateImageSurfaceSlot = 27;
constexpr size_t kCopyRectsSlot = 28;
// IDirect3DSurface8: IUnknown 0-2, IDirect3DSurface8 3-10.
constexpr size_t kGetDescSlot = 8;
constexpr size_t kLockRectSlot = 9;
constexpr size_t kUnlockRectSlot = 10;

template <typename Ret, typename... Args>
Ret CallSlot(IUnknown* object, size_t slot, Args... args) {
    using Fn = Ret(STDMETHODCALLTYPE*)(IUnknown*, Args...);
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[slot])(object, args...);
}

// D3DDISPLAYMODE kept its layout in D3D9.
inline HRESULT GetAdapterDisplayMode(Direct3D8* d3d, UINT adapter, D3DDISPLAYMODE* mode) {
    return CallSlot<HRESULT>(d3d, kGetAdapterDisplayModeSlot, adapter, mode);
}

inline HRESULT CreateDevice(Direct3D8* d3d, UINT adapter, D3DDEVTYPE type, HWND focus,
                            DWORD behavior, PresentParameters* params, Device8** device) {
    return CallSlot<HRESULT>(d3d, kCreateDeviceSlot, adapter, type, focus, behavior, params,
                             device);
}

inline HRESULT GetBackBuffer(Device8* device, UINT index, Surface8** surface) {
    return CallSlot<HRESULT>(device, kGetBackBufferSlot, index, D3DBACKBUFFER_TYPE_MONO, surface);
}

// Creates a system-memory surface.
inline HRESULT CreateImageSurface(Device8* device, UINT width, UINT height, D3DFORMAT format,
                                  Surface8** surface) {
    return CallSlot<HRESULT>(device, kCreateImageSurfaceSlot, width, height, format, surface);
}

// Copies all of `src` to the top-left of `dst`.
inline HRESULT CopyRects(Device8* device, Surface8* src, Surface8* dst) {
    return CallSlot<HRESULT>(device, kCopyRectsSlot, src, static_cast<const RECT*>(nullptr), 0u,
                             dst, static_cast<const POINT*>(nullptr));
}

inline HRESULT GetDesc(Surface8* surface, SurfaceDesc* desc) {
    return CallSlot<HRESULT>(surface, kGetDescSlot, desc);
}

inline HRESULT LockRect(Surface8* surface, D3DLOCKED_RECT* locked, DWORD flags) {
    return CallSlot<HRESULT>(surface, kLockRectSlot, locked, static_cast<const RECT*>(nullptr),
                             flags);
}

inline HRESULT UnlockRect(Surface8* surface) { return CallSlot<HRESULT>(surface, kUnlockRectSlot); }

}  // namespace gazeora::hook::d3d8
