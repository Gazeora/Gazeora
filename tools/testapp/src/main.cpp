// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Minimal "game" for testing the capture hook without touching a real game:
// a 1280x720 window with a color-cycling background and a moving white square, and in D3D11
// and D3D12 a depth-tested 3D scene, for 3D from depth, that the mouse looks around as in a
// first-person game (for head tracking; the window's title shows where it looks).
//
//   gazeora_testapp.exe            D3D11
//   gazeora_testapp.exe --d3d8     D3D8 (MinGW builds only: the Windows SDK has no d3d8.h;
//                                  in the x86 release only, as Windows' D3D8 is 32-bit)
//   gazeora_testapp.exe --d3d9     D3D9
//   gazeora_testapp.exe --d3d10    D3D10
//   gazeora_testapp.exe --d3d12    D3D12
//   gazeora_testapp.exe --d3d12-depth-write
//                                  D3D12, its depth buffer never leaving DEPTH_WRITE
//   gazeora_testapp.exe --opengl   OpenGL

#include <cstdio>
#include <cstring>

#include "testapp/app.h"
#include "testapp/mouse_look.h"

namespace {

struct Renderer {
    const char* flag;  // command-line switch; nullptr for the default
    const char* name;
    int (*run)(HWND);
};

const Renderer kRenderers[] = {
    {nullptr, "D3D11", testapp::RunD3D11},
#ifdef TESTAPP_HAVE_D3D8
    {"--d3d8", "D3D8", testapp::RunD3D8},
#endif
    {"--d3d9", "D3D9", testapp::RunD3D9},
    {"--d3d10", "D3D10", testapp::RunD3D10},
    {"--d3d12", "D3D12", testapp::RunD3D12},
    {"--d3d12-depth-write", "D3D12", testapp::RunD3D12DepthWrite},
    {"--opengl", "OpenGL", testapp::RunOpenGL},
};

}  // namespace

int main(int argc, char** argv) {
    const Renderer* renderer = &kRenderers[0];
    if (argc > 1) {
        renderer = nullptr;
        for (const Renderer& r : kRenderers) {
            if (r.flag && strcmp(argv[1], r.flag) == 0) {
                renderer = &r;
            }
        }
        if (!renderer) {
            fprintf(stderr, "unknown option %s\n", argv[1]);
            return 1;
        }
    }

    wchar_t title[64];
    swprintf(title, 64, L"Gazeora Test App (%hs)", renderer->name);
    HWND hwnd = testapp::CreateTestWindow(title);
    testapp::MouseLook look;
    look.Attach(hwnd);
    printf("Test app running (%s), PID %lu. Close the window to exit.\n", renderer->name,
           GetCurrentProcessId());
    return renderer->run(hwnd);
}
