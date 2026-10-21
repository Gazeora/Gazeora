#include "gazeora/hook/opengl/gl_hooks.h"

#include <windows.h>

#include "gazeora/hook/core/capture_target.h"
#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/opengl/gl_capture.h"

namespace gazeora::hook::opengl {

namespace {

using SwapBuffersFn = BOOL(WINAPI*)(HDC);
using SwapLayerBuffersFn = BOOL(WINAPI*)(HDC, UINT);
using DeleteContextFn = BOOL(WINAPI*)(HGLRC);
using GetCurrentContextFn = HGLRC(WINAPI*)();
using GetCurrentDCFn = HDC(WINAPI*)();
using MakeCurrentFn = BOOL(WINAPI*)(HDC, HGLRC);

SwapBuffersFn g_origSwapBuffers = nullptr;  // gdi32
SwapBuffersFn g_origWglSwapBuffers = nullptr;
SwapLayerBuffersFn g_origWglSwapLayerBuffers = nullptr;
DeleteContextFn g_origWglDeleteContext = nullptr;
GetCurrentContextFn g_wglGetCurrentContext = nullptr;
GetCurrentDCFn g_wglGetCurrentDC = nullptr;
MakeCurrentFn g_wglMakeCurrent = nullptr;

// gdi32's SwapBuffers calls opengl32's; don't capture the same frame twice.
thread_local bool t_inSwap = false;

// Only touched under a CaptureScope or TeardownLock.
GLCapture g_capture;

void CaptureFrame(HDC dc) {
    CaptureScope scope;
    if (!scope.Active()) {
        return;
    }
    HGLRC context = g_wglGetCurrentContext();
    HWND window = WindowFromDC(dc);
    // The blit reads the current context's back buffer, which must be this window's.
    if (!context || !window || WindowFromDC(g_wglGetCurrentDC()) != window) {
        return;
    }
    RECT client;
    if (!GetClientRect(window, &client) || client.right <= 0 || client.bottom <= 0) {
        return;  // minimized
    }
    if (scope.SelectTarget(window)) {
        g_capture.Capture(dc, context, UINT(client.right), UINT(client.bottom),
                          scope.PresentQpc());
    }
}

template <typename Fn>
BOOL Swap(HDC dc, Fn swap) {
    if (!t_inSwap) {
        CaptureFrame(dc);
    }
    bool outer = !t_inSwap;
    t_inSwap = true;
    BOOL result = swap();
    if (outer) {
        t_inSwap = false;
    }
    return result;
}

BOOL WINAPI HookSwapBuffers(HDC dc) {
    return Swap(dc, [&] { return g_origSwapBuffers(dc); });
}

BOOL WINAPI HookWglSwapBuffers(HDC dc) {
    return Swap(dc, [&] { return g_origWglSwapBuffers(dc); });
}

BOOL WINAPI HookWglSwapLayerBuffers(HDC dc, UINT planes) {
    if (!(planes & WGL_SWAP_MAIN_PLANE)) {
        return g_origWglSwapLayerBuffers(dc, planes);
    }
    return Swap(dc, [&] { return g_origWglSwapLayerBuffers(dc, planes); });
}

// Our GL objects live in the game's context; delete them while it still exists,
// making it current briefly if it isn't.
BOOL WINAPI HookWglDeleteContext(HGLRC context) {
    {
        TeardownLock lock;
        if (context && context == g_capture.Context()) {
            HGLRC previousContext = g_wglGetCurrentContext();
            HDC previousDc = g_wglGetCurrentDC();
            bool current = previousContext == context ||
                           (g_capture.Dc() && g_wglMakeCurrent(g_capture.Dc(), context));
            g_capture.Reset(current);
            if (current && previousContext != context) {
                g_wglMakeCurrent(previousDc, previousContext);
            }
        }
    }
    return g_origWglDeleteContext(context);
}

}  // namespace

bool Install() {
    HMODULE opengl32 = GraphicsRuntime(L"opengl32.dll");
    if (!opengl32) {
        return false;
    }
    g_wglGetCurrentContext =
        reinterpret_cast<GetCurrentContextFn>(ModuleExport(opengl32, "wglGetCurrentContext"));
    g_wglGetCurrentDC = reinterpret_cast<GetCurrentDCFn>(ModuleExport(opengl32, "wglGetCurrentDC"));
    g_wglMakeCurrent = reinterpret_cast<MakeCurrentFn>(ModuleExport(opengl32, "wglMakeCurrent"));
    void* swapBuffers = ModuleExport(GetModuleHandleW(L"gdi32.dll"), "SwapBuffers");
    void* wglSwapBuffers = ModuleExport(opengl32, "wglSwapBuffers");
    void* wglSwapLayerBuffers = ModuleExport(opengl32, "wglSwapLayerBuffers");
    void* wglDeleteContext = ModuleExport(opengl32, "wglDeleteContext");
    if (!g_wglGetCurrentContext || !g_wglGetCurrentDC || !g_wglMakeCurrent || !wglSwapBuffers ||
        !wglDeleteContext) {
        return false;
    }
    g_capture.Init(opengl32);

    return (!swapBuffers ||
            CreateHook("SwapBuffers", swapBuffers, &HookSwapBuffers, &g_origSwapBuffers)) &&
           CreateHook("wglSwapBuffers", wglSwapBuffers, &HookWglSwapBuffers,
                      &g_origWglSwapBuffers) &&
           (!wglSwapLayerBuffers ||
            CreateHook("wglSwapLayerBuffers", wglSwapLayerBuffers, &HookWglSwapLayerBuffers,
                       &g_origWglSwapLayerBuffers)) &&
           CreateHook("wglDeleteContext", wglDeleteContext, &HookWglDeleteContext,
                      &g_origWglDeleteContext);
}

}  // namespace gazeora::hook::opengl
