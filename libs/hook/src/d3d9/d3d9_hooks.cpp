#include "gazeora/hook/d3d9/d3d9_hooks.h"

#include <d3d9.h>
#include <wrl/client.h>

#include "gazeora/hook/core/capture_target.h"
#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/core/log.h"
#include "gazeora/hook/d3d9/d3d9_capture.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook::d3d9 {

namespace {

using Direct3DCreate9Fn = IDirect3D9*(WINAPI*)(UINT);
using Direct3DCreate9ExFn = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND,
                                              const RGNDATA*);
using PresentExFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9Ex*, const RECT*, const RECT*,
                                                HWND, const RGNDATA*, DWORD);
using SwapChainPresentFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DSwapChain9*, const RECT*,
                                                       const RECT*, HWND, const RGNDATA*, DWORD);
using ResetFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using ResetExFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9Ex*, D3DPRESENT_PARAMETERS*,
                                              D3DDISPLAYMODEEX*);

// IDirect3DDevice9: IUnknown 0-2, then 3-118 in d3d9.h order; IDirect3DDevice9Ex adds 119-133.
constexpr size_t kResetSlot = 16;
constexpr size_t kPresentSlot = 17;
constexpr size_t kPresentExSlot = 121;
constexpr size_t kResetExSlot = 132;
// IDirect3DSwapChain9: IUnknown 0-2, Present 3.
constexpr size_t kSwapChainPresentSlot = 3;

PresentFn g_origPresent = nullptr;
PresentExFn g_origPresentEx = nullptr;
SwapChainPresentFn g_origSwapChainPresent = nullptr;
ResetFn g_origReset = nullptr;
ResetExFn g_origResetEx = nullptr;
Direct3DCreate9Fn g_origDirect3DCreate9 = nullptr;
Direct3DCreate9ExFn g_origDirect3DCreate9Ex = nullptr;
volatile LONG g_vtablesHooked = 0;

// Device Present may go through the swapchain's; don't capture the same frame twice.
thread_local bool t_inPresent = false;

// Only touched under a CaptureScope or TeardownLock.
D3D9Capture g_capture;
IDirect3DSwapChain9* g_lastSwap = nullptr;  // identity only, not AddRef'd

void CaptureFrame(IDirect3DDevice9* device, IDirect3DSwapChain9* swap) {
    CaptureScope scope;
    if (!scope.Active()) {
        return;
    }
    ComPtr<IDirect3DDevice9> ownedDevice;
    ComPtr<IDirect3DSwapChain9> ownedSwap;
    if (swap) {
        if (FAILED(swap->GetDevice(&ownedDevice))) return;
        device = ownedDevice.Get();
    } else {
        if (FAILED(device->GetSwapChain(0, &ownedSwap))) return;
        swap = ownedSwap.Get();
    }
    if (!scope.SelectTarget(swap)) {
        return;
    }
    if (g_lastSwap != swap) {
        g_lastSwap = swap;
        g_capture.Reset();
    }
    ComPtr<IDirect3DSurface9> backBuffer;
    if (SUCCEEDED(swap->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer))) {
        g_capture.Capture(device, backBuffer.Get(), scope.PresentQpc());
    }
}

// Runs `present` with nested presents on this thread ignored.
template <typename Fn>
HRESULT Present(IDirect3DDevice9* device, IDirect3DSwapChain9* swap, Fn present) {
    if (!t_inPresent) {
        CaptureFrame(device, swap);
    }
    bool outer = !t_inPresent;
    t_inPresent = true;
    HRESULT hr = present();
    if (outer) {
        t_inPresent = false;
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE HookPresent(IDirect3DDevice9* device, const RECT* src, const RECT* dst,
                                      HWND window, const RGNDATA* dirty) {
    return Present(device, nullptr,
                   [&] { return g_origPresent(device, src, dst, window, dirty); });
}

HRESULT STDMETHODCALLTYPE HookPresentEx(IDirect3DDevice9Ex* device, const RECT* src,
                                        const RECT* dst, HWND window, const RGNDATA* dirty,
                                        DWORD flags) {
    return Present(device, nullptr,
                   [&] { return g_origPresentEx(device, src, dst, window, dirty, flags); });
}

HRESULT STDMETHODCALLTYPE HookSwapChainPresent(IDirect3DSwapChain9* swap, const RECT* src,
                                               const RECT* dst, HWND window, const RGNDATA* dirty,
                                               DWORD flags) {
    return Present(nullptr, swap, [&] {
        return g_origSwapChainPresent(swap, src, dst, window, dirty, flags);
    });
}

// Reset fails while D3DPOOL_DEFAULT resources (our MSAA resolve target) exist.
void ReleaseForReset(IDirect3DDevice9* device) {
    TeardownLock lock;
    if (g_capture.Device() == device) {
        g_capture.Reset();
        g_lastSwap = nullptr;
    }
}

HRESULT STDMETHODCALLTYPE HookReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
    ReleaseForReset(device);
    return g_origReset(device, params);
}

HRESULT STDMETHODCALLTYPE HookResetEx(IDirect3DDevice9Ex* device, D3DPRESENT_PARAMETERS* params,
                                      D3DDISPLAYMODEEX* mode) {
    ReleaseForReset(device);
    return g_origResetEx(device, params, mode);
}

// Creates a throwaway D3D9Ex device to read the device and swapchain vtables from;
// d3d9.dll uses the same ones for every device, Ex or not.
template <typename CreateFn>
bool HookVTables(Direct3DCreate9ExFn direct3DCreate9Ex, CreateFn create) {
    DummyWindow window;
    if (!window.hwnd()) {
        return false;
    }
    ComPtr<IDirect3D9Ex> d3d;
    HRESULT hr = direct3DCreate9Ex(D3D_SDK_VERSION, &d3d);
    if (FAILED(hr)) {
        SetStatus("dummy Direct3DCreate9Ex failed: 0x%08lX", hr);
        return false;
    }
    // Explicit size: the dummy window's client area may be empty.
    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth = 16;
    pp.BackBufferHeight = 16;
    pp.BackBufferCount = 1;
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.hDeviceWindow = window.hwnd();
    ComPtr<IDirect3DDevice9Ex> device;
    for (D3DDEVTYPE type : {D3DDEVTYPE_HAL, D3DDEVTYPE_NULLREF}) {
        hr = d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, type, window.hwnd(),
                                 D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES,
                                 &pp, nullptr, &device);
        if (SUCCEEDED(hr)) {
            break;
        }
    }
    ComPtr<IDirect3DSwapChain9> swap;
    if (SUCCEEDED(hr)) {
        hr = device->GetSwapChain(0, &swap);
    }
    if (FAILED(hr)) {
        SetStatus("dummy D3D9Ex device creation failed: 0x%08lX", hr);
        return false;
    }
    void* dev = device.Get();
    return create("IDirect3DDevice9::Present", VTableEntry(dev, kPresentSlot), &HookPresent,
                  &g_origPresent) &&
           create("IDirect3DDevice9Ex::PresentEx", VTableEntry(dev, kPresentExSlot),
                  &HookPresentEx, &g_origPresentEx) &&
           create("IDirect3DSwapChain9::Present", VTableEntry(swap.Get(), kSwapChainPresentSlot),
                  &HookSwapChainPresent, &g_origSwapChainPresent) &&
           create("IDirect3DDevice9::Reset", VTableEntry(dev, kResetSlot), &HookReset,
                  &g_origReset) &&
           create("IDirect3DDevice9Ex::ResetEx", VTableEntry(dev, kResetExSlot), &HookResetEx,
                  &g_origResetEx);
}

// Launched games: hook the vtables when the game first creates a Direct3D 9 object.
void OnDirect3DCreated() {
    if (InterlockedCompareExchange(&g_vtablesHooked, 1, 0) == 0 &&
        HookVTables(g_origDirect3DCreate9Ex,
                    [](const char* name, void* target, auto detour, auto* original) {
                        return CreateEnabledHook(name, target, detour, original);
                    })) {
        SetStatus("hooked D3D9 on first use, waiting for frames");
    }
}

IDirect3D9* WINAPI HookDirect3DCreate9(UINT sdkVersion) {
    IDirect3D9* d3d = g_origDirect3DCreate9(sdkVersion);
    if (d3d) {
        OnDirect3DCreated();
    }
    return d3d;
}

HRESULT WINAPI HookDirect3DCreate9Ex(UINT sdkVersion, IDirect3D9Ex** d3d) {
    HRESULT hr = g_origDirect3DCreate9Ex(sdkVersion, d3d);
    if (SUCCEEDED(hr)) {
        OnDirect3DCreated();
    }
    return hr;
}

}  // namespace

bool Install() {
    HMODULE module = GetModuleHandleW(L"d3d9.dll");
    if (module) {
        auto createEx =
            reinterpret_cast<Direct3DCreate9ExFn>(ModuleExport(module, "Direct3DCreate9Ex"));
        return createEx && HookVTables(createEx, [](const char* name, void* target, auto detour,
                                                    auto* original) {
                   return CreateHook(name, target, detour, original);
               });
    }
    module = GraphicsRuntime(L"d3d9.dll");
    void* create = ModuleExport(module, "Direct3DCreate9");
    void* createEx = ModuleExport(module, "Direct3DCreate9Ex");
    return create && createEx &&
           CreateHook("Direct3DCreate9", create, &HookDirect3DCreate9, &g_origDirect3DCreate9) &&
           CreateHook("Direct3DCreate9Ex", createEx, &HookDirect3DCreate9Ex,
                      &g_origDirect3DCreate9Ex);
}

}  // namespace gazeora::hook::d3d9
