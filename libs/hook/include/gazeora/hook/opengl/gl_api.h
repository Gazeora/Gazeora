// The OpenGL entry points the hook uses, loaded from the game's opengl32.dll (the hook
// doesn't link it) plus the constants it needs beyond GL 1.1, which is all the
// Windows SDK's GL/gl.h declares.
#pragma once

#include <windows.h>
#include <GL/gl.h>

#include <cstddef>
#include <cstdint>

namespace gazeora::hook::gl {

constexpr GLenum kReadFramebuffer = 0x8CA8;
constexpr GLenum kDrawFramebuffer = 0x8CA9;
constexpr GLenum kReadFramebufferBinding = 0x8CAA;
constexpr GLenum kDrawFramebufferBinding = 0x8CA6;
constexpr GLenum kColorAttachment0 = 0x8CE0;
constexpr GLenum kFramebufferComplete = 0x8CD5;
constexpr GLenum kFramebufferSrgb = 0x8DB9;
constexpr GLenum kPixelPackBuffer = 0x88EB;
constexpr GLenum kPixelPackBufferBinding = 0x88ED;
constexpr GLenum kStreamRead = 0x88E1;
constexpr GLenum kReadOnly = 0x88B8;
constexpr GLenum kBgra = 0x80E1;
constexpr GLenum kSyncGpuCommandsComplete = 0x9117;
constexpr GLenum kAlreadySignaled = 0x911A;
constexpr GLenum kConditionSatisfied = 0x911C;
constexpr GLenum kNumExtensions = 0x821D;
// GL_EXT_memory_object(_win32) and GL_EXT_semaphore(_win32).
constexpr GLenum kDedicatedMemoryObject = 0x9581;
constexpr GLenum kHandleTypeD3D11Image = 0x958B;
constexpr GLenum kHandleTypeD3D12Fence = 0x9594;
constexpr GLenum kD3D12FenceValue = 0x9595;
constexpr GLenum kLayoutGeneral = 0x958D;
constexpr GLenum kDeviceLuid = 0x9599;
constexpr GLenum kLuidSize = 8;

using GLsync = struct __GLsync*;
using GLsizeiptr = ptrdiff_t;
using GLuint64 = uint64_t;

struct Functions {
    // GL 1.1 and WGL, exported by opengl32.dll.
    PROC(WINAPI* wglGetProcAddress)(LPCSTR);
    HGLRC(WINAPI* wglGetCurrentContext)();
    HDC(WINAPI* wglGetCurrentDC)();
    BOOL(WINAPI* wglMakeCurrent)(HDC, HGLRC);
    GLenum(APIENTRY* GetError)();
    const GLubyte*(APIENTRY* GetString)(GLenum);
    void(APIENTRY* GetIntegerv)(GLenum, GLint*);
    GLboolean(APIENTRY* IsEnabled)(GLenum);
    void(APIENTRY* Enable)(GLenum);
    void(APIENTRY* Disable)(GLenum);
    void(APIENTRY* ReadBuffer)(GLenum);
    void(APIENTRY* ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
    void(APIENTRY* PixelStorei)(GLenum, GLint);
    void(APIENTRY* GenTextures)(GLsizei, GLuint*);
    void(APIENTRY* BindTexture)(GLenum, GLuint);
    void(APIENTRY* DeleteTextures)(GLsizei, const GLuint*);

    // GL 3.0+ / extensions, from wglGetProcAddress (needs a current context).
    const GLubyte*(APIENTRY* GetStringi)(GLenum, GLuint);
    void(APIENTRY* GenFramebuffers)(GLsizei, GLuint*);
    void(APIENTRY* DeleteFramebuffers)(GLsizei, const GLuint*);
    void(APIENTRY* BindFramebuffer)(GLenum, GLuint);
    void(APIENTRY* FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
    GLenum(APIENTRY* CheckFramebufferStatus)(GLenum);
    void(APIENTRY* BlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint,
                                    GLbitfield, GLenum);
    void(APIENTRY* GenBuffers)(GLsizei, GLuint*);
    void(APIENTRY* DeleteBuffers)(GLsizei, const GLuint*);
    void(APIENTRY* BindBuffer)(GLenum, GLuint);
    void(APIENTRY* BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
    void*(APIENTRY* MapBuffer)(GLenum, GLenum);
    GLboolean(APIENTRY* UnmapBuffer)(GLenum);
    GLsync(APIENTRY* FenceSync)(GLenum, GLbitfield);
    void(APIENTRY* DeleteSync)(GLsync);
    GLenum(APIENTRY* ClientWaitSync)(GLsync, GLbitfield, GLuint64);
    void(APIENTRY* Flush)();

    // GL_EXT_memory_object_win32 / GL_EXT_semaphore_win32: importing D3D11 textures
    // and fences.
    void(APIENTRY* GetUnsignedBytevEXT)(GLenum, GLubyte*);
    void(APIENTRY* CreateMemoryObjectsEXT)(GLsizei, GLuint*);
    void(APIENTRY* DeleteMemoryObjectsEXT)(GLsizei, const GLuint*);
    void(APIENTRY* MemoryObjectParameterivEXT)(GLuint, GLenum, const GLint*);
    void(APIENTRY* ImportMemoryWin32HandleEXT)(GLuint, GLuint64, GLenum, void*);
    void(APIENTRY* TexStorageMem2DEXT)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLuint,
                                       GLuint64);
    void(APIENTRY* GenSemaphoresEXT)(GLsizei, GLuint*);
    void(APIENTRY* DeleteSemaphoresEXT)(GLsizei, const GLuint*);
    void(APIENTRY* ImportSemaphoreWin32HandleEXT)(GLuint, GLenum, void*);
    void(APIENTRY* SemaphoreParameterui64vEXT)(GLuint, GLenum, const GLuint64*);
    void(APIENTRY* SignalSemaphoreEXT)(GLuint, GLuint, const GLuint*, GLuint, const GLuint*,
                                       const GLenum*);

    // Loads everything. The context they'll be used with must be current. False if it
    // lacks what both capture paths need (framebuffer objects, PBOs).
    bool Load(HMODULE opengl32);

    // True if the current context exposes `name`.
    bool HasExtension(const char* name) const;

    // Whether the D3D11 import extensions and fences were found.
    bool HasExternalObjects() const { return CreateMemoryObjectsEXT != nullptr; }
    bool HasSync() const { return FenceSync != nullptr; }
};

}  // namespace gazeora::hook::gl
