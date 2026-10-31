// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <windows.h>
#include <GL/gl.h>

#include <cstdio>

#include "testapp/app.h"

namespace testapp {

int RunOpenGL(HWND hwnd) {
    HDC dc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {sizeof(pfd), 1};
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int format = ChoosePixelFormat(dc, &pfd);
    if (!format || !SetPixelFormat(dc, format, &pfd)) {
        fprintf(stderr, "SetPixelFormat failed\n");
        return 1;
    }
    HGLRC context = wglCreateContext(dc);
    if (!context || !wglMakeCurrent(dc, context)) {
        fprintf(stderr, "wglCreateContext failed\n");
        return 1;
    }

    // Vsync like the other renderers, when the driver exposes wglSwapIntervalEXT.
    using SwapIntervalFn = BOOL(WINAPI*)(int);
    if (auto swapInterval = reinterpret_cast<SwapIntervalFn>(
            reinterpret_cast<void*>(wglGetProcAddress("wglSwapIntervalEXT")))) {
        swapInterval(1);
    }

    Clock clock;
    while (PumpMessages()) {
        Scene scene = SceneAt(clock.Seconds());
        glDisable(GL_SCISSOR_TEST);
        glClearColor(scene.background[0], scene.background[1], scene.background[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        // GL's origin is bottom-left. Scissor is left enabled across SwapBuffers on
        // purpose: the hook's copy must not be clipped by the game's GL state.
        glEnable(GL_SCISSOR_TEST);
        glScissor(scene.square.left, GLint(kHeight) - scene.square.bottom,
                  scene.square.right - scene.square.left, scene.square.bottom - scene.square.top);
        glClearColor(kWhite[0], kWhite[1], kWhite[2], kWhite[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        SwapBuffers(dc);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(context);
    ReleaseDC(hwnd, dc);
    return 0;
}

}  // namespace testapp
