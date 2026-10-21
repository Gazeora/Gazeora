#include "gazeora/hook/opengl/gl_api.h"

#include <cstring>
#include <type_traits>

namespace gazeora::hook::gl {

namespace {

template <typename Fn>
void LoadExport(HMODULE module, const char* name, Fn& fn) {
    fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(module, name)));
}

}  // namespace

bool Functions::Load(HMODULE opengl32) {
    *this = {};
    LoadExport(opengl32, "wglGetProcAddress", wglGetProcAddress);
    LoadExport(opengl32, "wglGetCurrentContext", wglGetCurrentContext);
    LoadExport(opengl32, "wglGetCurrentDC", wglGetCurrentDC);
    LoadExport(opengl32, "wglMakeCurrent", wglMakeCurrent);
    LoadExport(opengl32, "glGetError", GetError);
    LoadExport(opengl32, "glGetString", GetString);
    LoadExport(opengl32, "glGetIntegerv", GetIntegerv);
    LoadExport(opengl32, "glIsEnabled", IsEnabled);
    LoadExport(opengl32, "glEnable", Enable);
    LoadExport(opengl32, "glDisable", Disable);
    LoadExport(opengl32, "glReadBuffer", ReadBuffer);
    LoadExport(opengl32, "glReadPixels", ReadPixels);
    LoadExport(opengl32, "glPixelStorei", PixelStorei);
    LoadExport(opengl32, "glGenTextures", GenTextures);
    LoadExport(opengl32, "glBindTexture", BindTexture);
    LoadExport(opengl32, "glDeleteTextures", DeleteTextures);
    LoadExport(opengl32, "glFlush", Flush);
    if (!wglGetProcAddress || !GetString || !GetIntegerv || !IsEnabled || !Enable || !Disable ||
        !ReadBuffer || !ReadPixels || !PixelStorei || !GenTextures || !BindTexture || !DeleteTextures ||
        !Flush) {
        return false;
    }

    // Some drivers return small integers instead of null for unknown names.
    auto load = [this](const char* name, auto& fn) {
        PROC proc = wglGetProcAddress(name);
        auto value = reinterpret_cast<uintptr_t>(proc);
        fn = value > 3 && value != uintptr_t(-1)
                 ? reinterpret_cast<std::remove_reference_t<decltype(fn)>>(
                       reinterpret_cast<void*>(proc))
                 : nullptr;
        return fn != nullptr;
    };
    bool core = load("glGenFramebuffers", GenFramebuffers) &&
                load("glDeleteFramebuffers", DeleteFramebuffers) &&
                load("glBindFramebuffer", BindFramebuffer) &&
                load("glFramebufferTexture2D", FramebufferTexture2D) &&
                load("glCheckFramebufferStatus", CheckFramebufferStatus) &&
                load("glBlitFramebuffer", BlitFramebuffer) && load("glGenBuffers", GenBuffers) &&
                load("glDeleteBuffers", DeleteBuffers) && load("glBindBuffer", BindBuffer) &&
                load("glBufferData", BufferData) && load("glMapBuffer", MapBuffer) &&
                load("glUnmapBuffer", UnmapBuffer);
    if (!core) {
        return false;
    }
    load("glGetStringi", GetStringi);
    if (!(load("glFenceSync", FenceSync) && load("glDeleteSync", DeleteSync) &&
          load("glClientWaitSync", ClientWaitSync))) {
        FenceSync = nullptr;
    }
    if (HasExtension("GL_EXT_memory_object_win32") && HasExtension("GL_EXT_semaphore_win32") &&
        load("glGetUnsignedBytevEXT", GetUnsignedBytevEXT) &&
        load("glDeleteMemoryObjectsEXT", DeleteMemoryObjectsEXT) &&
        load("glMemoryObjectParameterivEXT", MemoryObjectParameterivEXT) &&
        load("glImportMemoryWin32HandleEXT", ImportMemoryWin32HandleEXT) &&
        load("glTexStorageMem2DEXT", TexStorageMem2DEXT) &&
        load("glGenSemaphoresEXT", GenSemaphoresEXT) &&
        load("glDeleteSemaphoresEXT", DeleteSemaphoresEXT) &&
        load("glImportSemaphoreWin32HandleEXT", ImportSemaphoreWin32HandleEXT) &&
        load("glSemaphoreParameterui64vEXT", SemaphoreParameterui64vEXT) &&
        load("glSignalSemaphoreEXT", SignalSemaphoreEXT)) {
        load("glCreateMemoryObjectsEXT", CreateMemoryObjectsEXT);  // marks it usable
    }
    return true;
}

bool Functions::HasExtension(const char* name) const {
    if (GetStringi) {
        GLint count = 0;
        GetIntegerv(kNumExtensions, &count);
        for (GLint i = 0; i < count; ++i) {
            auto ext = reinterpret_cast<const char*>(GetStringi(GL_EXTENSIONS, GLuint(i)));
            if (ext && strcmp(ext, name) == 0) {
                return true;
            }
        }
        if (count > 0) {
            return false;
        }
    }
    // Compatibility contexts before GL 3.0: one space-separated string.
    auto all = reinterpret_cast<const char*>(GetString(GL_EXTENSIONS));
    size_t length = strlen(name);
    for (const char* p = all; p && (p = strstr(p, name)) != nullptr; p += length) {
        bool start = p == all || p[-1] == ' ';
        bool end = p[length] == ' ' || p[length] == '\0';
        if (start && end) {
            return true;
        }
    }
    return false;
}

}  // namespace gazeora::hook::gl
