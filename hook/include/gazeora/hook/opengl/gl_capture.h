#pragma once

#include <windows.h>

#include <cstdint>

#include "gazeora/hook/d3d11/d3d11_bridge.h"
#include "gazeora/hook/opengl/gl_api.h"

namespace gazeora::hook {

// Copies the back buffer of an OpenGL context's default framebuffer into the frame
// ring of a D3D11 bridge.
//  - With GL_EXT_memory_object_win32 / GL_EXT_semaphore_win32, GL imports the ring's
//    textures and frame fence, blits the back buffer straight into a slot and signals
//    the fence itself: all on the GPU, nothing waits.
//  - Otherwise frames are read back asynchronously into pixel buffer objects and
//    uploaded from the CPU on a later swap.
// (WGL_NV_DX_interop2 isn't used: its lock waits for the GPU to go idle every frame.)
class GLCapture {
public:
    // The game's opengl32.dll, which GL functions are loaded from.
    void Init(HMODULE opengl32) { m_opengl32 = opengl32; }

    // Releases everything. GL objects are only deleted if `contextCurrent`; otherwise
    // they're left to be destroyed with their context.
    void Reset(bool contextCurrent);

    HGLRC Context() const { return m_context; }
    HDC Dc() const { return m_dc; }

    // Call inside SwapBuffers, with `context` current on `dc`.
    void Capture(HDC dc, HGLRC context, UINT width, UINT height, LONGLONG presentQpc);

private:
    static constexpr UINT kReadbacks = 3;

    struct Readback {
        GLuint buffer = 0;
        gl::GLsync fence = nullptr;
        LONGLONG presentQpc = 0;
        bool pending = false;
    };

    bool AttachContext(HGLRC context);
    const LUID* AdapterLuid() const { return m_hasAdapterLuid ? &m_adapterLuid : nullptr; }
    void BlitBackBuffer(GLuint fbo, UINT width, UINT height);

    // False means importing doesn't work here; use readback from now on.
    bool CaptureImported(UINT width, UINT height, LONGLONG presentQpc);
    bool ImportRing(D3D11FrameRing& ring);
    void ReleaseImports(bool contextCurrent);

    void CaptureReadback(UINT width, UINT height, LONGLONG presentQpc);
    void CreateReadbacks(UINT width, UINT height);
    void ReleaseReadbacks(bool contextCurrent);
    void UploadFinishedReadbacks();

    HMODULE m_opengl32 = nullptr;
    gl::Functions m_gl = {};
    HGLRC m_context = nullptr;
    HDC m_dc = nullptr;
    bool m_supported = false;
    bool m_hasSrgbControl = false;
    LUID m_adapterLuid = {};
    bool m_hasAdapterLuid = false;
    bool m_useImport = true;
    D3D11Bridge m_bridge;

    bool m_imported = false;
    uint32_t m_importedGeneration = 0;
    GLuint m_memory[kFrameSlots] = {};
    GLuint m_textures[kFrameSlots] = {};
    GLuint m_fbos[kFrameSlots] = {};
    GLuint m_frameFence = 0;  // GL semaphore

    UINT m_readbackWidth = 0;
    UINT m_readbackHeight = 0;
    Readback m_readbacks[kReadbacks];
    UINT m_nextReadback = 0;  // round-robin, so it's also the oldest pending readback
};

}  // namespace gazeora::hook
