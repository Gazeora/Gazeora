#include "gazeora/hook/opengl/gl_capture.h"

#include <cstdlib>
#include <cstring>

#include "gazeora/hook/core/log.h"

namespace gazeora::hook {

namespace {

// Imported slots are GL_RGBA8 textures; readback uses GL_BGRA, GL_UNSIGNED_BYTE.
constexpr DXGI_FORMAT kImportFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT kReadbackFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

}  // namespace

void GLCapture::Reset(bool contextCurrent) {
    ReleaseImports(contextCurrent);
    ReleaseReadbacks(contextCurrent);
    m_bridge.Reset();
    m_gl = {};
    m_context = nullptr;
    m_dc = nullptr;
    m_supported = false;
    m_hasAdapterLuid = false;
    m_useImport = true;
}

bool GLCapture::AttachContext(HGLRC context) {
    m_context = context;
    m_supported = m_gl.Load(m_opengl32);
    if (!m_supported) {
        SetStatus("OpenGL context lacks framebuffer objects or pixel buffers; not capturing it");
        return false;
    }
    auto version = reinterpret_cast<const char*>(m_gl.GetString(GL_VERSION));
    m_hasSrgbControl = (version && atoi(version) >= 3) ||
                       m_gl.HasExtension("GL_ARB_framebuffer_sRGB") ||
                       m_gl.HasExtension("GL_EXT_framebuffer_sRGB");

    // The GPU the context renders on, so imports open the bridge on the same one.
    if (m_gl.GetUnsignedBytevEXT) {
        GLubyte luid[gl::kLuidSize] = {};
        m_gl.GetUnsignedBytevEXT(gl::kDeviceLuid, luid);
        memcpy(&m_adapterLuid, luid, sizeof(m_adapterLuid));
        m_hasAdapterLuid = m_adapterLuid.LowPart != 0 || m_adapterLuid.HighPart != 0;
    }
    return true;
}

void GLCapture::Capture(HDC dc, HGLRC context, UINT width, UINT height, LONGLONG presentQpc) {
    if (context != m_context) {
        Reset(false);  // the previous context isn't current; its objects die with it
        AttachContext(context);
    }
    if (!m_supported) {
        return;
    }
    m_dc = dc;

    if (m_useImport) {
        if (CaptureImported(width, height, presentQpc)) {
            return;
        }
        ReleaseImports(true);
        m_useImport = false;
    }
    CaptureReadback(width, height, presentQpc);
}

// Blits the back buffer into `fbo`, flipped to D3D's top-down row order, leaving the
// game's GL state as it was.
void GLCapture::BlitBackBuffer(GLuint fbo, UINT width, UINT height) {
    GLint readFbo = 0, drawFbo = 0, readBuffer = GL_BACK;
    m_gl.GetIntegerv(gl::kReadFramebufferBinding, &readFbo);
    m_gl.GetIntegerv(gl::kDrawFramebufferBinding, &drawFbo);
    GLboolean scissor = m_gl.IsEnabled(GL_SCISSOR_TEST);
    GLboolean srgb = m_hasSrgbControl && m_gl.IsEnabled(gl::kFramebufferSrgb);

    m_gl.BindFramebuffer(gl::kReadFramebuffer, 0);
    m_gl.GetIntegerv(GL_READ_BUFFER, &readBuffer);
    m_gl.ReadBuffer(GL_BACK);
    m_gl.BindFramebuffer(gl::kDrawFramebuffer, fbo);
    if (scissor) m_gl.Disable(GL_SCISSOR_TEST);
    if (srgb) m_gl.Disable(gl::kFramebufferSrgb);

    m_gl.BlitFramebuffer(0, 0, GLint(width), GLint(height), 0, GLint(height), GLint(width), 0,
                         GL_COLOR_BUFFER_BIT, GL_NEAREST);

    if (srgb) m_gl.Enable(gl::kFramebufferSrgb);
    if (scissor) m_gl.Enable(GL_SCISSOR_TEST);
    m_gl.ReadBuffer(GLenum(readBuffer));
    m_gl.BindFramebuffer(gl::kReadFramebuffer, GLuint(readFbo));
    m_gl.BindFramebuffer(gl::kDrawFramebuffer, GLuint(drawFbo));
}

// ---- Importing the frame ring ----

bool GLCapture::ImportRing(D3D11FrameRing& ring) {
    ReleaseImports(true);

    GLint texture = 0, drawFbo = 0;
    m_gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    m_gl.GetIntegerv(gl::kDrawFramebufferBinding, &drawFbo);
    while (m_gl.GetError() != GL_NO_ERROR) {
    }

    bool complete = true;
    GLint dedicated = GL_TRUE;
    for (UINT i = 0; i < kFrameSlots; ++i) {
        m_gl.CreateMemoryObjectsEXT(1, &m_memory[i]);
        m_gl.MemoryObjectParameterivEXT(m_memory[i], gl::kDedicatedMemoryObject, &dedicated);
        // Size 0: a D3D11 texture's allocation size isn't known here, and the driver
        // takes it from the handle (a guessed size can be too small for its padding).
        m_gl.ImportMemoryWin32HandleEXT(m_memory[i], 0, gl::kHandleTypeD3D11Image,
                                        ring.SlotHandle(i));
        m_gl.GenTextures(1, &m_textures[i]);
        m_gl.BindTexture(GL_TEXTURE_2D, m_textures[i]);
        m_gl.TexStorageMem2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, GLsizei(ring.Width()),
                                GLsizei(ring.Height()), m_memory[i], 0);
        m_gl.GenFramebuffers(1, &m_fbos[i]);
        m_gl.BindFramebuffer(gl::kDrawFramebuffer, m_fbos[i]);
        m_gl.FramebufferTexture2D(gl::kDrawFramebuffer, gl::kColorAttachment0, GL_TEXTURE_2D,
                                  m_textures[i], 0);
        complete = complete &&
                   m_gl.CheckFramebufferStatus(gl::kDrawFramebuffer) == gl::kFramebufferComplete;
    }
    m_gl.GenSemaphoresEXT(1, &m_frameFence);
    m_gl.ImportSemaphoreWin32HandleEXT(m_frameFence, gl::kHandleTypeD3D12Fence,
                                       ring.FrameFenceHandle());
    GLenum error = m_gl.GetError();

    m_gl.BindTexture(GL_TEXTURE_2D, GLuint(texture));
    m_gl.BindFramebuffer(gl::kDrawFramebuffer, GLuint(drawFbo));
    if (!complete || error != GL_NO_ERROR) {
        SetStatus("importing D3D11 textures into OpenGL failed (GL error 0x%X); using readback",
                  error);
        return false;
    }
    m_imported = true;
    m_importedGeneration = ring.Generation();
    return true;
}

void GLCapture::ReleaseImports(bool contextCurrent) {
    if (contextCurrent && m_gl.HasExternalObjects()) {
        m_gl.DeleteFramebuffers(kFrameSlots, m_fbos);
        m_gl.DeleteTextures(kFrameSlots, m_textures);
        m_gl.DeleteMemoryObjectsEXT(kFrameSlots, m_memory);
        if (m_frameFence) m_gl.DeleteSemaphoresEXT(1, &m_frameFence);
    }
    for (UINT i = 0; i < kFrameSlots; ++i) {
        m_memory[i] = m_textures[i] = m_fbos[i] = 0;
    }
    m_frameFence = 0;
    m_imported = false;
}

bool GLCapture::CaptureImported(UINT width, UINT height, LONGLONG presentQpc) {
    if (!m_gl.HasExternalObjects()) {
        return false;
    }
    if (!m_bridge.Configure(AdapterLuid(), width, height, kImportFormat, kApiOpenGL)) {
        return true;  // already reported; readback would need the bridge too
    }
    D3D11FrameRing& ring = m_bridge.Ring();
    if ((!m_imported || m_importedGeneration != ring.Generation()) && !ImportRing(ring)) {
        return false;
    }

    LONG64 frame = NextFrameNumber();
    if (!ring.Claim(frame)) {
        return true;
    }
    UINT index = UINT(frame % kFrameSlots);
    BlitBackBuffer(m_fbos[index], width, height);
    // Signals the shared frame fence on the GPU once the blit is done; submitted by the
    // game's SwapBuffers right after this.
    gl::GLuint64 value = gl::GLuint64(frame);
    GLenum layout = gl::kLayoutGeneral;
    m_gl.SemaphoreParameterui64vEXT(m_frameFence, gl::kD3D12FenceValue, &value);
    m_gl.SignalSemaphoreEXT(m_frameFence, 0, nullptr, 1, &m_textures[index], &layout);
    FramePublished(frame, presentQpc);
    return true;
}

// ---- Readback through pixel buffer objects ----

void GLCapture::CreateReadbacks(UINT width, UINT height) {
    ReleaseReadbacks(true);
    GLint previous = 0;
    m_gl.GetIntegerv(gl::kPixelPackBufferBinding, &previous);
    for (Readback& readback : m_readbacks) {
        m_gl.GenBuffers(1, &readback.buffer);
        m_gl.BindBuffer(gl::kPixelPackBuffer, readback.buffer);
        m_gl.BufferData(gl::kPixelPackBuffer, gl::GLsizeiptr(width) * height * 4, nullptr,
                        gl::kStreamRead);
    }
    m_gl.BindBuffer(gl::kPixelPackBuffer, GLuint(previous));
    m_readbackWidth = width;
    m_readbackHeight = height;
}

void GLCapture::ReleaseReadbacks(bool contextCurrent) {
    for (Readback& readback : m_readbacks) {
        if (contextCurrent) {
            if (readback.fence) m_gl.DeleteSync(readback.fence);
            if (readback.buffer) m_gl.DeleteBuffers(1, &readback.buffer);
        }
        readback = Readback();
    }
    m_nextReadback = 0;
    m_readbackWidth = m_readbackHeight = 0;
}

void GLCapture::UploadFinishedReadbacks() {
    GLint previous = 0;
    m_gl.GetIntegerv(gl::kPixelPackBufferBinding, &previous);
    for (UINT i = 0; i < kReadbacks; ++i) {
        Readback& readback = m_readbacks[(m_nextReadback + i) % kReadbacks];
        if (!readback.pending) {
            continue;
        }
        if (readback.fence) {
            GLenum status = m_gl.ClientWaitSync(readback.fence, 0, 0);
            if (status != gl::kAlreadySignaled && status != gl::kConditionSatisfied) {
                break;  // later readbacks can't have finished either
            }
            m_gl.DeleteSync(readback.fence);
            readback.fence = nullptr;
        }
        readback.pending = false;
        m_gl.BindBuffer(gl::kPixelPackBuffer, readback.buffer);
        if (auto* data =
                static_cast<const uint8_t*>(m_gl.MapBuffer(gl::kPixelPackBuffer, gl::kReadOnly))) {
            // glReadPixels rows run bottom to top.
            m_bridge.UploadFrame(data, m_readbackWidth * 4, PixelLayout::BGRA8, readback.presentQpc,
                                 true);
            m_gl.UnmapBuffer(gl::kPixelPackBuffer);
        }
    }
    m_gl.BindBuffer(gl::kPixelPackBuffer, GLuint(previous));
}

void GLCapture::CaptureReadback(UINT width, UINT height, LONGLONG presentQpc) {
    if (!m_bridge.Configure(AdapterLuid(), width, height, kReadbackFormat, kApiOpenGL)) {
        return;
    }
    if (width != m_readbackWidth || height != m_readbackHeight) {
        CreateReadbacks(width, height);
    }

    UploadFinishedReadbacks();

    Readback& readback = m_readbacks[m_nextReadback];
    if (readback.pending) {
        CountSkippedFrame();  // the GPU is more than kReadbacks frames behind
        return;
    }

    GLint readFbo = 0, readBuffer = GL_BACK, packBuffer = 0;
    GLint alignment = 4, rowLength = 0, skipRows = 0, skipPixels = 0;
    m_gl.GetIntegerv(gl::kReadFramebufferBinding, &readFbo);
    m_gl.GetIntegerv(gl::kPixelPackBufferBinding, &packBuffer);
    m_gl.GetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    m_gl.GetIntegerv(GL_PACK_ROW_LENGTH, &rowLength);
    m_gl.GetIntegerv(GL_PACK_SKIP_ROWS, &skipRows);
    m_gl.GetIntegerv(GL_PACK_SKIP_PIXELS, &skipPixels);

    m_gl.BindFramebuffer(gl::kReadFramebuffer, 0);
    m_gl.GetIntegerv(GL_READ_BUFFER, &readBuffer);
    m_gl.ReadBuffer(GL_BACK);
    m_gl.BindBuffer(gl::kPixelPackBuffer, readback.buffer);
    m_gl.PixelStorei(GL_PACK_ALIGNMENT, 4);
    m_gl.PixelStorei(GL_PACK_ROW_LENGTH, 0);
    m_gl.PixelStorei(GL_PACK_SKIP_ROWS, 0);
    m_gl.PixelStorei(GL_PACK_SKIP_PIXELS, 0);

    m_gl.ReadPixels(0, 0, GLsizei(width), GLsizei(height), gl::kBgra, GL_UNSIGNED_BYTE, nullptr);
    readback.fence = m_gl.HasSync() ? m_gl.FenceSync(gl::kSyncGpuCommandsComplete, 0) : nullptr;

    m_gl.PixelStorei(GL_PACK_ALIGNMENT, alignment);
    m_gl.PixelStorei(GL_PACK_ROW_LENGTH, rowLength);
    m_gl.PixelStorei(GL_PACK_SKIP_ROWS, skipRows);
    m_gl.PixelStorei(GL_PACK_SKIP_PIXELS, skipPixels);
    m_gl.BindBuffer(gl::kPixelPackBuffer, GLuint(packBuffer));
    m_gl.ReadBuffer(GLenum(readBuffer));
    m_gl.BindFramebuffer(gl::kReadFramebuffer, GLuint(readFbo));

    readback.presentQpc = presentQpc;
    readback.pending = true;
    m_nextReadback = (m_nextReadback + 1) % kReadbacks;
}

}  // namespace gazeora::hook
